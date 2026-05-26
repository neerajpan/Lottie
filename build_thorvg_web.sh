#!/usr/bin/env bash
#
# ThorVG Web/WASM Build Script
# Builds ThorVG as a static library for Godot's Web (Emscripten/WASM) export.
#
# Requirements:
#   - Emscripten SDK activated (source emsdk_env.sh)
#   - meson and ninja installed
#
# Usage:
#   source /path/to/emsdk/emsdk_env.sh   # activate Emscripten
#   ./build_thorvg_web.sh
#
# Output:
#   thirdparty/thorvg/build_wasm/src/libthorvg.a

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "========================================"
echo "Building ThorVG for Web (WASM)"
echo "========================================"

# Verify Emscripten is active
if ! command -v emcc >/dev/null 2>&1; then
    echo "ERROR: emcc not found. Activate Emscripten first:"
    echo "  source /path/to/emsdk/emsdk_env.sh"
    exit 1
fi
echo "Emscripten: $(emcc --version | head -1)"

# Resolve meson
if command -v meson >/dev/null 2>&1; then
    MESON="meson"
elif python3 -m mesonbuild.mesonmain --version >/dev/null 2>&1; then
    MESON="python3 -m mesonbuild.mesonmain"
else
    echo "ERROR: meson not found. Install with: pip3 install --user meson ninja"
    exit 1
fi
echo "Meson: $($MESON --version)"

THORVG_DIR="$SCRIPT_DIR/thirdparty/thorvg"
BUILD_DIR="$THORVG_DIR/build_wasm"

# Write Emscripten cross-file for Meson
CROSS_FILE="$THORVG_DIR/emscripten.ini"
cat > "$CROSS_FILE" <<EOF
[binaries]
c     = 'emcc'
cpp   = 'em++'
ar    = 'emar'
strip = 'emstrip'

[host_machine]
system     = 'emscripten'
cpu_family = 'wasm32'
cpu        = 'wasm32'
endian     = 'little'
EOF

echo "Cross-file written: $CROSS_FILE"

cd "$THORVG_DIR"

# Clean previous build
if [ -d "$BUILD_DIR" ]; then
    echo "Cleaning previous WASM build..."
    rm -rf "$BUILD_DIR"
fi

echo "Configuring ThorVG for WASM..."
$MESON setup "$BUILD_DIR" \
    --cross-file "$CROSS_FILE" \
    -Dbuildtype=release \
    -Doptimization=3 \
    -Db_ndebug=true \
    -Ddefault_library=static \
    -Dthreads=false \
    -Dsimd=false \
    -Dpartial=true \
    -Dengines=sw \
    -Dloaders=lottie \
    -Dbindings=capi \
    -Dexamples=false \
    -Dtests=false \
    --backend=ninja \
    --wipe

echo
echo "Building ThorVG WASM..."
$MESON compile -C "$BUILD_DIR" -j"$(nproc 2>/dev/null || echo 4)"

echo
echo "========================================"
echo "ThorVG WASM build complete!"
echo "Output: $BUILD_DIR/src/libthorvg.a"
echo "========================================"
