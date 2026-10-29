#!/usr/bin/env python3
"""
Anvil Inspector — UWP package analysis tool.

Extracts an .appx or .msix bundle and parses its AppxManifest.xml into a
structured JSON report covering identity, entry points, dependencies, and
capabilities.
"""

import argparse
import json
import sys
import zipfile
from pathlib import Path
from xml.etree import ElementTree as ET

# All standard UWP manifest namespace prefixes.
NS = {
    "appx":   "http://schemas.microsoft.com/appx/manifest/foundation/windows10",
    "mp":     "http://schemas.microsoft.com/appx/2014/phone/manifest",
    "uap":    "http://schemas.microsoft.com/appx/manifest/uap/windows10",
    "uap2":   "http://schemas.microsoft.com/appx/manifest/uap/windows10/2",
    "uap3":   "http://schemas.microsoft.com/appx/manifest/uap/windows10/3",
    "uap4":   "http://schemas.microsoft.com/appx/manifest/uap/windows10/4",
    "uap5":   "http://schemas.microsoft.com/appx/manifest/uap/windows10/5",
    "rescap": "http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities",
    "desktop": "http://schemas.microsoft.com/appx/manifest/desktop/windows10",
    "com":    "http://schemas.microsoft.com/appx/manifest/com/windows10",
    "iot":    "http://schemas.microsoft.com/appx/manifest/iot/windows10",
    "mobile": "http://schemas.microsoft.com/appx/manifest/mobile/windows10",
}

# Register all prefixes so ElementTree serialises them cleanly.
for prefix, uri in NS.items():
    ET.register_namespace(prefix, uri)


def _attr(element, *attr_names: str) -> str | None:
    """Return the first matching attribute from a list of candidates."""
    for name in attr_names:
        value = element.get(name)
        if value is not None:
            return value
    return None


def _find(root: ET.Element, *tag_path: str) -> ET.Element | None:
    """XPath-style search tolerant of missing namespace prefixes."""
    for tag in tag_path:
        result = root.find(tag, NS)
        if result is not None:
            return result
    return None


def _findall(root: ET.Element, *tag_paths: str) -> list[ET.Element]:
    """findall across multiple candidate XPath expressions."""
    results: list[ET.Element] = []
    for tag in tag_paths:
        results.extend(root.findall(tag, NS))
    return results


# ---------------------------------------------------------------------------
# Extraction
# ---------------------------------------------------------------------------

def extract_package(package_path: Path, output_dir: Path) -> Path:
    """
    Unzips an .appx / .msix archive into output_dir.

    For .msixbundle / .appxbundle files the outer archive is extracted first,
    then the first inner .appx/.msix is extracted into a sub-directory.
    """
    suffix = package_path.suffix.lower()
    bundle_suffixes = {".msixbundle", ".appxbundle"}

    output_dir.mkdir(parents=True, exist_ok=True)

    if suffix in bundle_suffixes:
        # Extract the bundle envelope, then drill into the first sub-package.
        bundle_root = output_dir / "bundle"
        bundle_root.mkdir(exist_ok=True)
        with zipfile.ZipFile(package_path) as zf:
            zf.extractall(bundle_root)

        sub_packages = list(bundle_root.glob("*.appx")) + list(bundle_root.glob("*.msix"))
        if not sub_packages:
            raise FileNotFoundError("No .appx/.msix found inside bundle archive.")

        inner_pkg = sub_packages[0]
        extract_root = output_dir / inner_pkg.stem
        extract_root.mkdir(exist_ok=True)
        with zipfile.ZipFile(inner_pkg) as zf:
            zf.extractall(extract_root)
        return extract_root

    extract_root = output_dir / package_path.stem
    extract_root.mkdir(exist_ok=True)
    with zipfile.ZipFile(package_path) as zf:
        zf.extractall(extract_root)
    return extract_root


# ---------------------------------------------------------------------------
# Manifest parsing
# ---------------------------------------------------------------------------

def _parse_identity(root: ET.Element) -> dict:
    identity = _find(root, "appx:Identity", "Identity")
    if identity is None:
        return {}
    return {
        "Name":               identity.get("Name"),
        "Version":            identity.get("Version"),
        "Publisher":          identity.get("Publisher"),
        "ProcessorArchitecture": identity.get("ProcessorArchitecture", "neutral"),
    }


def _parse_properties(root: ET.Element) -> dict:
    props = _find(root, "appx:Properties", "Properties")
    if props is None:
        return {}

    def text(tag: str) -> str | None:
        el = _find(props, f"appx:{tag}", tag)
        return el.text.strip() if el is not None and el.text else None

    return {
        "DisplayName":    text("DisplayName"),
        "PublisherDisplayName": text("PublisherDisplayName"),
        "Description":    text("Description"),
        "Logo":           text("Logo"),
    }


def _parse_target_device_families(root: ET.Element) -> list[dict]:
    deps = _find(root, "appx:Dependencies", "Dependencies")
    if deps is None:
        return []
    families = []
    for tdf in _findall(deps, "appx:TargetDeviceFamily", "TargetDeviceFamily"):
        families.append({
            "Name":           tdf.get("Name"),
            "MinVersion":     tdf.get("MinVersion"),
            "MaxVersionTested": tdf.get("MaxVersionTested"),
        })
    return families


def _parse_package_dependencies(root: ET.Element) -> list[dict]:
    deps = _find(root, "appx:Dependencies", "Dependencies")
    if deps is None:
        return []
    result = []
    for pd in _findall(
        deps,
        "appx:PackageDependency",
        "PackageDependency",
        "uap:PackageDependency",
    ):
        result.append({
            "Name":               pd.get("Name"),
            "MinVersion":         pd.get("MinVersion"),
            "Publisher":          pd.get("Publisher"),
        })
    return result


def _parse_framework_dependencies(root: ET.Element) -> list[dict]:
    """
    Extracts framework / extension SDK references — these are the most
    interesting for Anvil because they reveal WinUI / XAML linkage.
    """
    deps = _find(root, "appx:Dependencies", "Dependencies")
    if deps is None:
        return []
    result = []
    for fd in _findall(deps, "appx:Sdk", "Sdk", "appx:Framework", "Framework"):
        result.append({
            "Name":    fd.get("Name"),
            "Version": fd.get("Version"),
            "Uri":     fd.get("Uri"),
        })
    return result


def _parse_capabilities(root: ET.Element) -> dict:
    caps_el = _find(root, "appx:Capabilities", "Capabilities")
    if caps_el is None:
        return {"standard": [], "device": [], "restricted": []}

    standard, device, restricted = [], [], []

    for child in caps_el:
        tag = child.tag.split("}")[-1]  # strip namespace URI
        name = child.get("Name", tag)
        if "restrictedcapabilities" in child.tag:
            restricted.append(name)
        elif "DeviceCapability" in child.tag or tag == "DeviceCapability":
            device.append(name)
        else:
            standard.append(name)

    return {"standard": standard, "device": device, "restricted": restricted}


def _parse_applications(root: ET.Element) -> list[dict]:
    """
    Parses every <Application> element — the most critical section for Anvil
    because it reveals the executable entry point and the UI framework in use.
    """
    apps_el = _find(root, "appx:Applications", "Applications")
    if apps_el is None:
        return []

    apps = []
    for app in _findall(apps_el, "appx:Application", "Application"):
        entry = {
            "Id":           app.get("Id"),
            "Executable":   app.get("Executable"),
            "EntryPoint":   app.get("EntryPoint"),      # e.g. "App" (managed) or "Windows.FullTrustApplication"
            "StartPage":    app.get("StartPage"),        # for hosted web apps
            "TrustLevel":   app.get("TrustLevel"),
            "RuntimeBehavior": app.get("RuntimeBehavior"),
            "VisualElements": None,
            "Extensions": [],
        }

        # Visual elements describe the tile / splash — also tells us XAML usage.
        ve = _find(app, "uap:VisualElements", "appx:VisualElements", "VisualElements")
        if ve is not None:
            entry["VisualElements"] = {
                "DisplayName":       ve.get("DisplayName"),
                "BackgroundColor":   ve.get("BackgroundColor"),
                "Square150x150Logo": ve.get("Square150x150Logo"),
                "Square44x44Logo":   ve.get("Square44x44Logo"),
                "Description":       ve.get("Description"),
            }

        # Extensions can reveal COM registrations, desktop bridges, etc.
        for ext_container in _findall(app, "appx:Extensions", "Extensions", "uap:Extensions"):
            for ext in ext_container:
                tag  = ext.tag.split("}")[-1]
                cat  = ext.get("Category", tag)
                ext_info = {"Category": cat, "Executable": ext.get("Executable")}
                # Pull desktop:FullTrustProcess if present (Desktop Bridge indicator)
                ftp = _find(ext, "desktop:FullTrustProcess")
                if ftp is not None:
                    ext_info["FullTrustExecutable"] = ftp.get("Executable")
                entry["Extensions"].append(ext_info)

        apps.append(entry)

    return apps


def _classify_ui_framework(manifest_report: dict) -> dict:
    """
    Heuristic classification of the UI framework based on manifest clues.
    This is one of the core pieces of intelligence Anvil needs to route
    rendering calls appropriately.
    """
    clues = []
    framework = "unknown"

    apps = manifest_report.get("Applications", [])
    deps = [d.get("Name", "") for d in manifest_report.get("PackageDependencies", [])]

    for app in apps:
        ep = (app.get("EntryPoint") or "").lower()
        exe = (app.get("Executable") or "").lower()

        if "microsoft.ui.xaml" in " ".join(deps).lower():
            framework = "WinUI3"
            clues.append("PackageDependency on Microsoft.UI.Xaml")
        elif ep in ("app", "windows.fulltrusttaskhost") or (
            # Managed UWP: EntryPoint is a dotted C# type name (e.g. "Calculator.App",
            # "MyNamespace.App") — never contains a path separator or file extension.
            ep and "." in ep and not ep.endswith(".exe") and "/" not in ep and "\\" not in ep
            and ep not in ("windows.fulltrusteapplication", "windows.fulltrustapplication")
        ):
            framework = "UWP/XAML (managed)"
            clues.append(f"Managed EntryPoint (C# type): {app.get('EntryPoint')}")
        elif ep in ("windows.fulltrusteapplication", "windows.fulltrustapplication"):
            framework = "Desktop Bridge (Win32 wrapped)"
            clues.append("FullTrustApplication entry point")
        elif exe.endswith(".exe") and not ep:
            framework = "Win32 (bare executable)"
            clues.append("Executable present without managed EntryPoint")

    return {"detected_framework": framework, "evidence": clues}


def parse_manifest(manifest_path: Path) -> dict:
    tree = ET.parse(manifest_path)
    root = tree.getroot()

    report: dict = {
        "Identity":             _parse_identity(root),
        "Properties":           _parse_properties(root),
        "TargetDeviceFamilies": _parse_target_device_families(root),
        "PackageDependencies":  _parse_package_dependencies(root),
        "FrameworkDependencies": _parse_framework_dependencies(root),
        "Capabilities":         _parse_capabilities(root),
        "Applications":         _parse_applications(root),
    }

    report["UIFrameworkClassification"] = _classify_ui_framework(report)
    return report


# ---------------------------------------------------------------------------
# File inventory
# ---------------------------------------------------------------------------

def inventory_package(extract_root: Path) -> dict:
    """
    Walks the extracted package and categorises files by type.
    Useful for identifying PE binaries, WinMD type metadata, and XAML assets.
    """
    inventory: dict[str, list[str]] = {
        "pe_binaries":   [],   # .exe, .dll
        "winmd":         [],   # Windows Metadata type libraries
        "xaml":          [],   # XAML markup
        "priconfig":     [],   # Package Resource Index
        "certificates":  [],   # .p7x, .cer embedded certificates
        "other":         [],
    }

    for p in sorted(extract_root.rglob("*")):
        if p.is_dir():
            continue
        rel = str(p.relative_to(extract_root))
        suf = p.suffix.lower()
        if suf in (".exe", ".dll"):
            inventory["pe_binaries"].append(rel)
        elif suf == ".winmd":
            inventory["winmd"].append(rel)
        elif suf == ".xaml":
            inventory["xaml"].append(rel)
        elif suf in (".pri", ".scale-100", ".scale-200"):
            inventory["priconfig"].append(rel)
        elif suf in (".p7x", ".cer", ".pfx"):
            inventory["certificates"].append(rel)
        else:
            inventory["other"].append(rel)

    return inventory


# ---------------------------------------------------------------------------
# CLI entry point
# ---------------------------------------------------------------------------

def build_report(package_path: Path, output_dir: Path) -> dict:
    print(f"[anvil] Extracting {package_path.name} …")
    extract_root = extract_package(package_path, output_dir)
    print(f"[anvil] Extracted to: {extract_root}")

    manifest_path = extract_root / "AppxManifest.xml"
    if not manifest_path.exists():
        # Some bundles place the manifest one level deeper.
        candidates = list(extract_root.rglob("AppxManifest.xml"))
        if not candidates:
            raise FileNotFoundError(f"AppxManifest.xml not found under {extract_root}")
        manifest_path = candidates[0]

    print(f"[anvil] Parsing manifest: {manifest_path}")
    manifest_data = parse_manifest(manifest_path)

    print("[anvil] Inventorying package files …")
    file_inventory = inventory_package(extract_root)

    report = {
        "anvil_inspector_version": "0.1.0",
        "source_package":          str(package_path.resolve()),
        "extracted_to":            str(extract_root.resolve()),
        "manifest":                manifest_data,
        "file_inventory":          file_inventory,
    }
    return report


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Anvil Inspector — analyse a UWP .appx/.msix package.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python anvil_inspector.py Calculator.msix
  python anvil_inspector.py MyApp.appx --output ./reports --pretty
  python anvil_inspector.py bundle.msixbundle --output ./out --save report.json
""",
    )
    parser.add_argument("package", type=Path, help="Path to .appx / .msix / bundle file")
    parser.add_argument(
        "--output", "-o", type=Path, default=Path("./anvil_extracted"),
        help="Directory for extracted package contents (default: ./anvil_extracted)",
    )
    parser.add_argument(
        "--save", "-s", type=Path, default=None,
        help="Write the JSON report to this file instead of stdout",
    )
    parser.add_argument(
        "--pretty", action="store_true", default=True,
        help="Pretty-print JSON output (default: true)",
    )
    parser.add_argument(
        "--compact", action="store_true",
        help="Compact JSON output (overrides --pretty)",
    )
    args = parser.parse_args()

    package_path: Path = args.package.resolve()
    if not package_path.exists():
        print(f"[anvil] ERROR: File not found: {package_path}", file=sys.stderr)
        sys.exit(1)

    indent = None if args.compact else 2
    report = build_report(package_path, args.output.resolve())
    json_output = json.dumps(report, indent=indent, ensure_ascii=False)

    if args.save:
        args.save.write_text(json_output, encoding="utf-8")
        print(f"[anvil] Report written to: {args.save.resolve()}")
    else:
        print(json_output)


if __name__ == "__main__":
    main()
