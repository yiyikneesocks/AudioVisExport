#!/usr/bin/env bash
# =============================================================================
# build_win_cross.sh — Cross-compile AudioVisExport to Windows .exe from WSL
#
# Uses Clang (MSVC ABI) + lld-link + xwin Windows SDK
#
# Prerequisites (one-time setup):
#   sudo apt-get install -y clang lld
#   curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
#   source "$HOME/.cargo/env"
#   cargo install xwin
#   xwin --accept-license --arch x86_64 --sdk-version 10.0.22621 \
#     splat --output "$HOME/.xwin-sysroot"
#
# Usage:
#   bash scripts/build_win_cross.sh              # Build only
#   bash scripts/build_win_cross.sh --deploy     # Build + deploy to Windows test dir
#   bash scripts/build_win_cross.sh --clean      # Clean and rebuild
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build_win"
XWIN_SYSROOT="$HOME/.xwin-sysroot"
DEST_DIR="/mnt/c/Users/yiyikneesocks/Desktop/AudioVisExport_test"
DEPLOY=false
CLEAN=false

# Parse arguments
for arg in "$@"; do
    case "$arg" in
        --deploy) DEPLOY=true ;;
        --clean)  CLEAN=true ;;
        --help|-h)
            echo "Usage: $0 [--deploy] [--clean]"
            echo "  --deploy  Deploy .exe files to Windows test directory"
            echo "  --clean   Clean build directory before building"
            exit 0
            ;;
    esac
done

echo "============================================"
echo " AudioVisExport Windows Cross-Build"
echo "============================================"

# --- Step 1: Environment check ---
echo ""
echo "[1/6] Checking environment..."

check_cmd() {
    command -v "$1" >/dev/null 2>&1 || { echo "ERROR: $1 not found. $2"; exit 1; }
}

check_cmd clang     "Install: sudo apt-get install clang lld"
check_cmd lld-link  "Install: sudo apt-get install clang lld"
check_cmd cmake     "Install: sudo apt-get install cmake"
check_cmd ninja     "Install: sudo apt-get install ninja-build"

if [ ! -d "$XWIN_SYSROOT" ]; then
    echo "ERROR: xwin sysroot not found at $XWIN_SYSROOT"
    echo "Run: xwin --accept-license --arch x86_64 --sdk-version 10.0.22621 splat --output $XWIN_SYSROOT"
    exit 1
fi

# Fix case-sensitivity symlinks (Windows FS is case-insensitive, Linux is not).
# JUCE includes <Dbghelp.h>/<D2d1.lib> etc. — exact casing differs from SDK files.
fix_case_links() {
    local inc="$XWIN_SYSROOT/sdk/include/um"
    local lib="$XWIN_SYSROOT/sdk/lib/um/x86_64"
    ln -sfn DbgHelp.h  "$inc/Dbghelp.h"   2>/dev/null
    ln -sfn d2d1.lib   "$lib/D2d1.lib"    2>/dev/null
    ln -sfn dwrite.lib "$lib/Dwrite.lib"  2>/dev/null
    ln -sfn dcomp.lib  "$lib/DComp.lib"   2>/dev/null
    ln -sfn dbghelp.lib "$lib/DbgHelp.lib" 2>/dev/null
    ln -sfn dbghelp.lib "$lib/Dbghelp.lib" 2>/dev/null
}
fix_case_links

echo "  clang:    $(clang-18 --version 2>&1 | head -1)"
echo "  lld-link: $(lld-link-18 --version 2>&1 | head -1)"
echo "  cmake:    $(cmake --version 2>&1 | head -1)"
echo "  xwin:     $XWIN_SYSROOT"

# --- Step 2: Clean if requested ---
if [ "$CLEAN" = true ] && [ -d "$BUILD_DIR" ]; then
    echo ""
    echo "[2/6] Cleaning build directory..."
    rm -rf "$BUILD_DIR"
else
    echo ""
    echo "[2/6] Build directory: $BUILD_DIR"
fi

# --- Step 3: CMake Configure ---
echo ""
echo "[3/6] Configuring CMake..."

cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$PROJECT_DIR/cmake/toolchains/clang-cl-msvc.cmake" \
    -DAVX_USE_LOCAL_JUCE=ON

# --- Step 4: Build ---
echo ""
echo "[4/6] Building..."

# Use -j2 to avoid OOM on JUCE large TUs (same as native Linux build)
cmake --build "$BUILD_DIR" -j2 2>&1 | tee "$BUILD_DIR/build_log.txt"

# --- Step 5: Verify ---
echo ""
echo "[5/6] Verifying build artifacts..."

CLI_EXE="$BUILD_DIR/AudioVisExport_artefacts/Release/AudioVisExport.exe"
GUI_EXE="$BUILD_DIR/AudioVisGUI_artefacts/Release/AudioVisGUI.exe"

if [ -f "$CLI_EXE" ]; then
    echo "  CLI: $CLI_EXE"
    file "$CLI_EXE"
else
    echo "ERROR: CLI .exe not found at $CLI_EXE"
    exit 1
fi

if [ -f "$GUI_EXE" ]; then
    echo "  GUI: $GUI_EXE"
    file "$GUI_EXE"
else
    echo "WARNING: GUI .exe not found at $GUI_EXE (CLI-only build succeeded)"
fi

# --- Step 6: Deploy ---
if [ "$DEPLOY" = true ]; then
    echo ""
    echo "[6/6] Deploying to Windows test directory..."

    if [ ! -d "$DEST_DIR" ]; then
        mkdir -p "$DEST_DIR"
    fi

    # v0.5.4 #1''（用户方案）：时间戳部署——每次落 AudioVisGUI_MMDDHHmm.exe，
    # 并尝试清理旧版本（被 Windows 写锁占用则下次再删）；启动 bat 指向最新时间戳。
    TS=$(date +%m%d%H%M)

    # 尝试删除旧的时间戳 exe 与占用失败遗留的 _new.exe（占用就跳过，下次部署再清）
    for old in "$DEST_DIR"/AudioVisGUI_[0-9]*.exe "$DEST_DIR"/AudioVisExport_[0-9]*.exe \
               "$DEST_DIR"/AudioVisGUI_new.exe "$DEST_DIR"/AudioVisExport_new.exe \
               "$DEST_DIR"/AudioVisGUI.exe "$DEST_DIR"/AudioVisExport.exe; do
        [ -e "$old" ] || continue
        rm -f "$old" 2>/dev/null || echo "  [deploy] ${old##*/} 被占用，跳过（下次部署再删）"
    done

    cp "$CLI_EXE" "$DEST_DIR/AudioVisExport_$TS.exe"
    if [ -f "$GUI_EXE" ]; then
        cp "$GUI_EXE" "$DEST_DIR/AudioVisGUI_$TS.exe"
    fi

    # Generate .bat launcher（启动最新时间戳的 GUI）
    cat > "$DEST_DIR/Run_AudioVisGUI.bat" <<EOF
@echo off
setlocal enabledelayedexpansion
set "LATEST="
for /f "delims=" %%F in ('dir /b /o:n "%~dp0AudioVisGUI_*.exe" 2^>nul') do set "LATEST=%%F"
if defined LATEST (start "" "%~dp0!LATEST!") else (echo No AudioVisGUI_*.exe found & pause)
EOF

    echo "  Deployed to: $DEST_DIR"
    echo "  Run: double-click Run_AudioVisGUI.bat on Windows"
else
    echo ""
    echo "[6/6] Skipping deploy (use --deploy to enable)"
fi

echo ""
echo "============================================"
echo " Build complete!"
echo "============================================"
