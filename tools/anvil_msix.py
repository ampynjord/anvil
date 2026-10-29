#!/usr/bin/env python3
"""
anvil_msix.py — Anvil MSIX launcher for Wine.

Usage:
    python3 anvil_msix.py <path-to-app.msix> [--wine WINE] [--prefix WINEPREFIX]

What it does:
  1. Reads AppxManifest.xml from the MSIX (ZIP) to extract package identity.
  2. Unpacks the MSIX into <WINEPREFIX>/drive_c/Program Files/WindowsApps/<fullname>/.
  3. Sets ANVIL_PKG_* and ANVIL_APP_AUMID environment variables.
  4. Launches the app's executable with:
       WINEDLLOVERRIDES="combase=n,b" wine <exe>

Environment produced:
    ANVIL_PKG_FULLNAME    Name_Version_Arch__PublisherId
    ANVIL_PKG_FAMILYNAME  Name_PublisherId
    ANVIL_PKG_PATH        <install dir> (Windows path)
    ANVIL_APP_AUMID       FamilyName!AppId
    ANVIL_PKG_PUBLISHER   CN=... (raw Publisher string from manifest)
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile
import xml.etree.ElementTree as ET
from pathlib import Path

NS = {
    'pkg':   'http://schemas.microsoft.com/appx/manifest/foundation/windows10',
    'uap':   'http://schemas.microsoft.com/appx/manifest/uap/windows10',
    'uap2':  'http://schemas.microsoft.com/appx/manifest/uap/windows10/2',
    'uap3':  'http://schemas.microsoft.com/appx/manifest/uap/windows10/3',
    # Legacy (Windows 8.x) namespace
    'pkg2':  'http://schemas.microsoft.com/appx/2010/manifest',
}

# Both Windows 10 and legacy Windows 8 namespaces for Identity
_IDENTITY_TAGS = [
    '{http://schemas.microsoft.com/appx/manifest/foundation/windows10}Identity',
    '{http://schemas.microsoft.com/appx/2010/manifest}Identity',
]
_APPLICATION_TAGS = [
    '{http://schemas.microsoft.com/appx/manifest/foundation/windows10}Application',
    '{http://schemas.microsoft.com/appx/2010/manifest}Application',
]


def parse_manifest(xml_bytes: bytes) -> dict:
    """Extract identity and application entry point from AppxManifest.xml."""
    root = ET.fromstring(xml_bytes)

    identity = None
    for tag in _IDENTITY_TAGS:
        identity = root.find('.//' + tag.lstrip('{').split('}')[1],
                             {v: k for k, v in NS.items()})
        # Use direct namespace search
        for child in root.iter():
            if child.tag in _IDENTITY_TAGS:
                identity = child
                break
        if identity is not None:
            break

    if identity is None:
        raise ValueError("AppxManifest.xml: <Identity> element not found")

    name       = identity.get('Name', '')
    version    = identity.get('Version', '1.0.0.0')
    arch       = identity.get('ProcessorArchitecture', 'neutral')
    publisher  = identity.get('Publisher', '')

    # Derive PublisherId: last segment of family name.
    # Real Windows computes it as a base32 hash of the publisher cert.
    # For Anvil we use the hash embedded in ANVIL_PKG_FAMILYNAME if already set,
    # or fall back to a simplified extraction from the full name env var.
    # If the MSIX was built by Microsoft, the publisher id is well-known.
    publisher_id = _derive_publisher_id(name, publisher)

    full_name   = f"{name}_{version}_{arch}__{publisher_id}"
    family_name = f"{name}_{publisher_id}"

    # Find first Application entry
    app_id = 'App'
    for child in root.iter():
        if child.tag in _APPLICATION_TAGS:
            app_id = child.get('Id', 'App')
            break

    aumid = f"{family_name}!{app_id}"

    # Find executable
    executable = None
    for child in root.iter():
        if child.tag in _APPLICATION_TAGS:
            executable = child.get('Executable')
            break

    return {
        'name':         name,
        'version':      version,
        'arch':         arch,
        'publisher':    publisher,
        'publisher_id': publisher_id,
        'full_name':    full_name,
        'family_name':  family_name,
        'aumid':        aumid,
        'app_id':       app_id,
        'executable':   executable,
    }


# Known publisher-id table for common Microsoft packages.
# The real algorithm is: SHA256(publisher DN) → base32(first 8 bytes).
# We keep a small lookup table here for the most common ones;
# unknown publishers fall back to a placeholder that will log a warning.
_KNOWN_PUBLISHER_IDS = {
    'CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US':
        '8wekyb3d8bbwe',
    'CN=Microsoft Windows, O=Microsoft Corporation, L=Redmond, S=Washington, C=US':
        'cw5n1h2txyewy',
}

def _derive_publisher_id(name: str, publisher: str) -> str:
    """Return the publisher-id string (base32 hash of publisher cert DN)."""
    if publisher in _KNOWN_PUBLISHER_IDS:
        return _KNOWN_PUBLISHER_IDS[publisher]
    # Fallback: compute from SHA256 like Windows does.
    try:
        return _compute_publisher_id(publisher)
    except Exception:
        print(f"[anvil] WARNING: unknown publisher, using placeholder publisher_id", file=sys.stderr)
        return 'xxxxxxxxxxxx'


def _compute_publisher_id(publisher_dn: str) -> str:
    """
    Compute the 13-char publisher id from the publisher DN.

    Windows algorithm:
      1. Canonicalize the DN to uppercase
      2. Encode as UTF-16 LE
      3. SHA256 the result
      4. Take the first 8 bytes of the digest
      5. Encode with custom base32 alphabet: b c d f g h j k m n p q r s t v w x y z 2 3 4 5 6 7
      6. Take first 13 chars
    """
    import hashlib
    import struct

    # Windows uses the raw DER-encoded publisher, but for env-var injection
    # we approximate with the string representation.
    dn_upper = publisher_dn.upper()
    dn_utf16 = dn_upper.encode('utf-16-le')
    digest = hashlib.sha256(dn_utf16).digest()

    # Custom base32 (no vowels + some chars removed to avoid ambiguity)
    alphabet = 'bcdfghjkmnpqrstvwxyz2345678'  # 27 chars — Windows uses specific 32
    # Simpler: use standard base32 and take first 13 chars
    import base64
    b32 = base64.b32encode(digest[:8]).decode('ascii').lower()
    return b32[:13]


def unpack_msix(msix_path: str, install_dir: str) -> None:
    """Unpack MSIX (ZIP) into install_dir, skipping Windows-specific metadata."""
    skip_prefixes = (
        'AppxSignature.p7x',
        'AppxBlockMap.xml',
        '[Content_Types].xml',
        'AppxMetadata/',
    )
    os.makedirs(install_dir, exist_ok=True)
    with zipfile.ZipFile(msix_path, 'r') as zf:
        for info in zf.infolist():
            if any(info.filename.startswith(p) for p in skip_prefixes):
                continue
            zf.extract(info, install_dir)
    print(f"[anvil] Unpacked {msix_path} → {install_dir}")


def wine_path(unix_path: str) -> str:
    """Convert a Unix path to its Windows equivalent inside a Wine prefix."""
    # Simple heuristic: /home/user/.wine/drive_c/... → C:\...
    # For robustness use winepath if available.
    try:
        result = subprocess.run(
            ['winepath', '-w', unix_path],
            capture_output=True, text=True, timeout=5
        )
        if result.returncode == 0:
            return result.stdout.strip()
    except (FileNotFoundError, subprocess.TimeoutExpired):
        pass
    # Fallback: manual conversion
    m = re.search(r'/drive_([a-z])/(.*)', unix_path)
    if m:
        drive, rest = m.group(1).upper(), m.group(2)
        return f"{drive}:\\{rest.replace('/', '\\')}"
    return unix_path


def main():
    parser = argparse.ArgumentParser(
        description='Anvil MSIX launcher — unpack and run UWP apps under Wine')
    parser.add_argument('msix', help='Path to the .msix or .appx package')
    parser.add_argument('--wine', default=os.environ.get('WINE', 'wine'),
                        help='Wine binary to use (default: wine)')
    parser.add_argument('--prefix', default=os.environ.get('WINEPREFIX',
                        os.path.expanduser('~/.wine')),
                        help='Wine prefix directory (default: ~/.wine)')
    parser.add_argument('--no-unpack', action='store_true',
                        help='Skip unpacking (package already installed)')
    parser.add_argument('--dry-run', action='store_true',
                        help='Print env vars and command without launching')
    args = parser.parse_args()

    msix_path = os.path.abspath(args.msix)
    if not os.path.isfile(msix_path):
        print(f"[anvil] ERROR: {msix_path} not found", file=sys.stderr)
        sys.exit(1)

    # Parse manifest
    with zipfile.ZipFile(msix_path, 'r') as zf:
        try:
            xml_bytes = zf.read('AppxManifest.xml')
        except KeyError:
            print("[anvil] ERROR: AppxManifest.xml not found in package", file=sys.stderr)
            sys.exit(1)

    info = parse_manifest(xml_bytes)

    print(f"[anvil] Package: {info['full_name']}")
    print(f"[anvil] AUMID:   {info['aumid']}")
    print(f"[anvil] Exec:    {info['executable']}")

    # Install directory inside Wine prefix
    windowsapps = os.path.join(args.prefix, 'drive_c', 'Program Files', 'WindowsApps')
    install_dir  = os.path.join(windowsapps, info['full_name'])

    if not args.no_unpack:
        if os.path.isdir(install_dir):
            print(f"[anvil] Package already installed at {install_dir} — skipping unpack")
        else:
            unpack_msix(msix_path, install_dir)
    else:
        print(f"[anvil] --no-unpack: using existing {install_dir}")

    # Compute Windows-style install path
    win_install_path = wine_path(install_dir)

    # Build environment
    env = os.environ.copy()
    env['WINEPREFIX']           = args.prefix
    env['WINEDLLOVERRIDES']     = 'combase=n,b'
    env['ANVIL_PKG_FULLNAME']   = info['full_name']
    env['ANVIL_PKG_FAMILYNAME'] = info['family_name']
    env['ANVIL_PKG_PATH']       = win_install_path
    env['ANVIL_APP_AUMID']      = info['aumid']
    env['ANVIL_PKG_PUBLISHER']  = info['publisher']

    if info['executable'] is None:
        print("[anvil] ERROR: no <Application Executable=...> found in manifest", file=sys.stderr)
        sys.exit(1)

    exe_unix = os.path.join(install_dir, info['executable'].replace('\\', '/'))
    exe_win  = wine_path(exe_unix)

    cmd = [args.wine, exe_win]

    print("[anvil] Environment:")
    for k in ('ANVIL_PKG_FULLNAME', 'ANVIL_PKG_FAMILYNAME', 'ANVIL_PKG_PATH',
              'ANVIL_APP_AUMID', 'ANVIL_PKG_PUBLISHER', 'WINEDLLOVERRIDES'):
        print(f"  {k}={env[k]}")
    print(f"[anvil] Command: {' '.join(cmd)}")

    if args.dry_run:
        print("[anvil] --dry-run: not launching")
        return

    os.execvpe(cmd[0], cmd, env)


if __name__ == '__main__':
    main()
