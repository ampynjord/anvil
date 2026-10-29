#!/usr/bin/env bash
# run_calculator.sh — Build Anvil combase stub, run Calculator.exe under Wine,
#                     and capture WinRT factory activation logs.
#
# Run inside the anvil-wine Docker container:
#   docker run --rm -v $PWD:/anvil anvil-wine

set -uo pipefail

ANVIL=/anvil
CALC_DIR=$ANVIL/anvil_extracted/Calculator_test
STUBS=$ANVIL/stubs
OUT=$STUBS/out
WINE_PFX=/root/.wine_anvil

info()  { echo "[anvil] $*"; }
sep()   { echo "[anvil] ══════════════════════════════════════════════════"; }

# ── 1. Xvfb (headless display) ───────────────────────────────────────────────
info "Starting Xvfb on :99 ..."
Xvfb :99 -screen 0 1024x768x24 -ac &
XVFB_PID=$!
export DISPLAY=:99
sleep 1
info "Xvfb PID=$XVFB_PID"

# ── 2. Build combase_anvil64.dll + test harnesses ────────────────────────────
sep
info "Building combase_anvil64.dll + test harnesses ..."
mkdir -p "$OUT"
cd "$STUBS"
make -f Makefile.wine 64 tests 2>&1
ls -lh "$OUT/combase_anvil64.dll"

# ── 3. Initialise Wine prefix (64-bit, headless) ─────────────────────────────
sep
info "Initialising Wine prefix at $WINE_PFX ..."
export WINEPREFIX=$WINE_PFX
export WINEARCH=win64
export WINEDEBUG=-all        # suppress Wine's own spam

WINE=wine64-stable

# Init Wine prefix (runs wineboot.exe internally)
$WINE wineboot 2>&1 || true
sleep 3
info "Wine prefix ready."

# ── 4. Install our DLL as combase.dll ────────────────────────────────────────
sep
info "Installing Anvil combase stub ..."
cp "$OUT/combase_anvil64.dll" "$WINE_PFX/drive_c/windows/system32/combase.dll"
info "Installed $(stat -c%s $WINE_PFX/drive_c/windows/system32/combase.dll) bytes"

# ── 5a. Run test_pkg_identity.exe — package identity hooks ───────────────────
sep
info "Running test_pkg_identity.exe ..."
sep
WINEDEBUG=-all \
WINEDLLOVERRIDES="combase=n,b" \
ANVIL_PKG_FULLNAME="Microsoft.WindowsCalculator_10.2103.8.0_x64__8wekyb3d8bbwe" \
ANVIL_PKG_FAMILYNAME="Microsoft.WindowsCalculator_8wekyb3d8bbwe" \
ANVIL_PKG_PATH="C:\\Program Files\\WindowsApps\\Microsoft.WindowsCalculator_10.2103.8.0_x64__8wekyb3d8bbwe" \
ANVIL_APP_AUMID="Microsoft.WindowsCalculator_8wekyb3d8bbwe!App" \
ANVIL_PKG_PUBLISHER="CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US" \
timeout 15 $WINE "$OUT/test_pkg_identity.exe" 2>&1 || true

# ── 5b. Run test_activation_chain.exe — validates all 13 WinRT factories ─────
sep
info "Running test_activation_chain.exe ..."
sep
WINEDEBUG=-all \
WINEDLLOVERRIDES="combase=n,b" \
timeout 15 $WINE "$OUT/test_activation_chain.exe" 2>&1 || true

# ── 5c. Run test_core_window.exe — CoreApplication::Run() lifecycle ──────────
sep
info "Running test_core_window.exe ..."
sep
WINEDEBUG=-all \
WINEDLLOVERRIDES="combase=n,b" \
timeout 20 $WINE "$OUT/test_core_window.exe" 2>&1 || true

# ── 5d. Run test_p4_stubs.exe — statics interfaces ───────────────────────────
sep
info "Running test_p4_stubs.exe ..."
sep
WINEDEBUG=-all \
WINEDLLOVERRIDES="combase=n,b" \
timeout 15 $WINE "$OUT/test_p4_stubs.exe" 2>&1 || true

# ── 5e. Run test_p5_stubs.exe — Window/Application/DependencyProperty statics ─
sep
info "Running test_p5_stubs.exe ..."
sep
WINEDEBUG=-all \
WINEDLLOVERRIDES="combase=n,b" \
timeout 15 $WINE "$OUT/test_p5_stubs.exe" 2>&1 || true

# ── 5f. Run test_storage_stubs.exe — ApplicationData + StorageFolder ─────────
sep
info "Running test_storage_stubs.exe ..."
sep
WINEDEBUG=-all \
WINEDLLOVERRIDES="combase=n,b" \
ANVIL_PKG_FAMILYNAME="Microsoft.WindowsCalculator_8wekyb3d8bbwe" \
timeout 15 $WINE "$OUT/test_storage_stubs.exe" 2>&1 || true

# ── 5g. Run test_system_stubs.exe — DisplayInfo, Package, ThreadPool, Dispatcher
sep
info "Running test_system_stubs.exe ..."
sep
WINEDEBUG=-all \
WINEDLLOVERRIDES="combase=n,b" \
ANVIL_PKG_FULLNAME="Microsoft.WindowsCalculator_10.2103.8.0_x64__8wekyb3d8bbwe" \
ANVIL_PKG_FAMILYNAME="Microsoft.WindowsCalculator_8wekyb3d8bbwe" \
ANVIL_PKG_PATH="C:\\Program Files\\WindowsApps\\Microsoft.WindowsCalculator_10.2103.8.0_x64__8wekyb3d8bbwe" \
ANVIL_PKG_PUBLISHER="CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US" \
timeout 15 $WINE "$OUT/test_system_stubs.exe" 2>&1 || true

sep
info "All runs complete."
sep
