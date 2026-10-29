#!/usr/bin/env bash
# setup_anvil_vm.sh — Bootstrap Anvil on a fresh Zorin OS / Ubuntu VM.
#
# Downloads source from the host HTTP server, compiles, runs tests, and
# installs the combase stub into Wine.
#
# Usage (run in the Zorin VM terminal):
#   curl -fsSL http://HOST_IP:8765/setup_anvil_vm.sh | bash
#
# HOST_IP is the Windows Hyper-V host IP visible from the VM —
# typically 172.26.112.1 or 172.22.176.1

set -euo pipefail

# ── Config ──────────────────────────────────────────────────────────────────
ANVIL_DIR="$HOME/anvil"
HOST_IP="${ANVIL_HOST_IP:-}"   # set via env or auto-detected below
PORT=8765
WINE_PREFIX="$HOME/.wine_anvil"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
info()  { echo -e "${GREEN}[anvil]${NC} $*"; }
warn()  { echo -e "${YELLOW}[anvil]${NC} $*"; }
error() { echo -e "${RED}[anvil] ERROR:${NC} $*" >&2; exit 1; }

# ── Auto-detect host IP (default route = Hyper-V host) ──────────────────────
if [[ -z "$HOST_IP" ]]; then
    HOST_IP=$(ip route show default 2>/dev/null | awk '/default via/ {print $3; exit}')
    [[ -z "$HOST_IP" ]] && error "Cannot auto-detect host IP. Set ANVIL_HOST_IP=<ip> and re-run."
    info "Auto-detected host IP: $HOST_IP"
fi
BASE_URL="http://$HOST_IP:$PORT/stubs"

# ── Step 1: Install dependencies ─────────────────────────────────────────────
info "Installing dependencies..."
sudo apt-get update -qq
sudo apt-get install -y --no-install-recommends \
    gcc \
    mingw-w64 \
    wine \
    wine64 \
    winetricks \
    python3 \
    git \
    curl \
    2>&1 | grep -E "^(Get:|Setting up|Processing)" || true

# Confirm Wine and GCC are available
command -v wine   >/dev/null || error "wine not found after install"
command -v gcc    >/dev/null || error "gcc not found after install"
command -v x86_64-w64-mingw32-gcc >/dev/null || error "mingw-w64 not found"
info "Dependencies OK"

# ── Step 2: Download Anvil source from host ───────────────────────────────────
info "Downloading Anvil stubs from $BASE_URL ..."
mkdir -p "$ANVIL_DIR/stubs/out"

FILES=(
    winrt_types.h
    combase_anvil.c
    test_combase_anvil.c
    test_wine_harness.c
    Makefile.wine
)
for f in "${FILES[@]}"; do
    info "  ← $f"
    curl -fsSL "$BASE_URL/$f" -o "$ANVIL_DIR/stubs/$f" \
        || error "Failed to download $f from $BASE_URL/$f"
done
info "Download complete"

cd "$ANVIL_DIR/stubs"

# ── Step 3: Native Linux unit tests ──────────────────────────────────────────
info "Building native unit tests..."
gcc -O0 -g -Wall -Wextra -Wno-unused-parameter \
    -I. -o out/test_combase \
    combase_anvil.c test_combase_anvil.c
info "Running native unit tests..."
echo "────────────────────────────────"
./out/test_combase
echo "────────────────────────────────"
info "Native tests passed ✓"

# ── Step 4: Cross-compile combase_anvil.dll (64-bit Wine override) ───────────
info "Cross-compiling combase_anvil.dll (x86-64) ..."
x86_64-w64-mingw32-gcc \
    -O2 -Wall -Wno-unused-parameter \
    -I. \
    -shared \
    -Wl,--kill-at \
    -o out/combase_anvil64.dll \
    combase_anvil.c \
    -lkernel32 -lmingwex -lmsvcrt -nostdlib \
    2>&1 || {
        warn "Shared build failed (expected without full Wine headers), trying static object..."
        x86_64-w64-mingw32-gcc -O2 -Wall -Wno-unused-parameter -I. \
            -c -o out/combase_anvil.o combase_anvil.c
        info "Static object compiled OK (full DLL needs Wine build tree for linking)"
    }
[[ -f out/combase_anvil64.dll ]] && info "DLL built: out/combase_anvil64.dll ✓" \
    || warn "DLL not built (needs Wine build tree) — continuing with native tests only"

# ── Step 5: Cross-compile Wine harness .exe ───────────────────────────────────
info "Cross-compiling Wine test harness..."
x86_64-w64-mingw32-gcc \
    -O0 -g \
    -o out/test_wine_harness.exe \
    test_wine_harness.c \
    -lkernel32 -luser32 && info "Harness built: out/test_wine_harness.exe ✓" \
    || warn "Harness build failed — skipping Wine DLL override test"

# ── Step 6: Install stub DLL into Wine prefix and run harness ─────────────────
if [[ -f out/combase_anvil64.dll && -f out/test_wine_harness.exe ]]; then
    info "Setting up Wine prefix: $WINE_PREFIX"
    export WINEPREFIX="$WINE_PREFIX"
    export WINEARCH=win64
    # Initialise prefix silently
    WINEDLLOVERRIDES="" wineboot --init 2>/dev/null || true

    info "Installing combase_anvil.dll as combase override..."
    cp out/combase_anvil64.dll "$WINE_PREFIX/drive_c/windows/system32/combase.dll"

    info "Running Wine test harness with Anvil combase override..."
    echo "────────────────────────────────"
    WINEPREFIX="$WINE_PREFIX" \
    WINEDLLOVERRIDES="combase=n,b" \
    WINEDEBUG=-all \
        wine out/test_wine_harness.exe 2>&1 || true
    echo "────────────────────────────────"
else
    info "Skipping Wine DLL override test (DLL or harness not available)"
fi

# ── Step 7: Summary ───────────────────────────────────────────────────────────
info "──────────────────────────────────────────"
info "Anvil VM setup complete"
info "  Source:  $ANVIL_DIR/stubs/"
info "  Binaries: $ANVIL_DIR/stubs/out/"
info ""
info "Next steps:"
info "  1. Check the [MISSING FACTORY] lines above — those are the WinRT"
info "     classes Anvil needs to stub for Calculator."
info "  2. Place a Calculator.exe in stubs/out/ and run:"
info "     WINEPREFIX=$WINE_PREFIX WINEDLLOVERRIDES='combase=b,n' wine stubs/out/Calculator.exe"
info "──────────────────────────────────────────"
