#!/usr/bin/env bash
#
# iOS Extension Build Script (macOS host with Xcode required)
# Builds the LottieAnimation GDExtension as a .xcframework containing
# iOS device + iOS Simulator slices, ready to be referenced by the
# .gdextension as `ios.debug` / `ios.release`.
#
# Requires that ThorVG has already been built for iOS:
#   ./build_thorvg_ios.sh
#
# Optional env knobs:
#   TARGETS  -- space-separated subset of {template_debug template_release}
#               (default builds both)
#   VARIANTS -- space-separated subset of {device sim-arm64 sim-x86_64}
#               (default builds all three)
#
# Outputs:
#   demo/addons/godot_lottie/bin/libgodot_lottie.ios.template_debug.xcframework/
#   demo/addons/godot_lottie/bin/libgodot_lottie.ios.template_release.xcframework/

set -e

if [ "$(uname -s)" != "Darwin" ]; then
    echo "ERROR: iOS builds require a macOS host with Xcode."
    exit 1
fi

if ! command -v xcodebuild >/dev/null 2>&1; then
    echo "ERROR: xcodebuild not found. Install Xcode (full install, not just CLT)."
    exit 1
fi

TARGETS="${TARGETS:-template_debug template_release}"
VARIANTS="${VARIANTS:-device sim-arm64 sim-x86_64}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BIN_DIR="$SCRIPT_DIR/demo/addons/godot_lottie/bin"
INTERMEDIATES="$BIN_DIR/ios_intermediates"

mkdir -p "$INTERMEDIATES"

# Resolve SCons invocation. `brew install scons` on macOS provides a
# standalone `scons` binary but NOT a Python `SCons` module, so
# `python3 -m SCons` fails there. Conversely, `pip install --user scons`
# on Linux makes both available. Prefer the `scons` script when present.
if command -v scons >/dev/null 2>&1; then
    SCONS="scons"
elif python3 -m SCons --version >/dev/null 2>&1; then
    SCONS="python3 -m SCons"
elif python -m SCons --version >/dev/null 2>&1; then
    SCONS="python -m SCons"
else
    echo "ERROR: SCons not found. Install with:"
    echo "  brew install scons              (macOS)"
    echo "  pip3 install --user scons       (anywhere)"
    exit 1
fi
echo "Using SCons: $SCONS"

# ---- Verify ThorVG iOS builds exist --------------------------------------

require_thorvg() {
    local NAME="$1"
    local LIB="$SCRIPT_DIR/thirdparty/thorvg/builddir_$NAME/src/libthorvg.a"
    if [ ! -f "$LIB" ]; then
        echo "ERROR: ThorVG iOS build missing: $LIB"
        echo "       Run: ./build_thorvg_ios.sh"
        exit 1
    fi
}

for v in $VARIANTS; do
    case "$v" in
        device)      require_thorvg ios-arm64 ;;
        sim-arm64)   require_thorvg ios-arm64-simulator ;;
        sim-x86_64)  require_thorvg ios-x86_64-simulator ;;
    esac
done

# ---- Build each (target, variant) combo via SCons ------------------------

build_variant() {
    local TARGET="$1"  # template_debug / template_release
    local VARIANT="$2" # device / sim-arm64 / sim-x86_64

    local ARCH SIM_FLAG OUT_NAME THORVG_BUILDDIR
    case "$VARIANT" in
        device)
            ARCH="arm64"; SIM_FLAG="ios_simulator=no"
            OUT_NAME="libgodot_lottie.ios.$TARGET.arm64.a"
            THORVG_BUILDDIR="$SCRIPT_DIR/thirdparty/thorvg/builddir_ios-arm64"
            ;;
        sim-arm64)
            ARCH="arm64"; SIM_FLAG="ios_simulator=yes"
            OUT_NAME="libgodot_lottie.ios.$TARGET.arm64.simulator.a"
            THORVG_BUILDDIR="$SCRIPT_DIR/thirdparty/thorvg/builddir_ios-arm64-simulator"
            ;;
        sim-x86_64)
            ARCH="x86_64"; SIM_FLAG="ios_simulator=yes"
            OUT_NAME="libgodot_lottie.ios.$TARGET.x86_64.simulator.a"
            THORVG_BUILDDIR="$SCRIPT_DIR/thirdparty/thorvg/builddir_ios-x86_64-simulator"
            ;;
    esac

    echo
    echo "=== SCons: ios $TARGET $VARIANT (arch=$ARCH $SIM_FLAG) ==="

    cd "$SCRIPT_DIR"
    $SCONS platform=ios "target=$TARGET" "arch=$ARCH" "$SIM_FLAG" \
        dlink_enabled=no \
        -j"$(sysctl -n hw.ncpu)"

    # SCons now produces a .a (StaticLibrary). Locate it in bin/.
    echo "bin/ contents after SCons:"
    ls -la "$BIN_DIR" 2>&1 | head -20
    local PRODUCED
    PRODUCED="$(ls "$BIN_DIR"/libgodot_lottie.ios.${TARGET}*.a 2>/dev/null | head -1)"
    if [ -z "$PRODUCED" ] || [ ! -f "$PRODUCED" ]; then
        echo "ERROR: SCons did not produce a .a for ios $TARGET $VARIANT"
        exit 1
    fi

    # Find the godot-cpp static library produced by SConscript.
    # It lives in thirdparty/godot-cpp/bin/ with the same target/arch suffix.
    local GODOT_CPP_LIB
    GODOT_CPP_LIB="$(ls "$SCRIPT_DIR/thirdparty/godot-cpp/bin/"libgodot-cpp.ios.${TARGET}.${ARCH}*.a 2>/dev/null | head -1)"
    if [ -z "$GODOT_CPP_LIB" ] || [ ! -f "$GODOT_CPP_LIB" ]; then
        echo "ERROR: godot-cpp .a not found in thirdparty/godot-cpp/bin/"
        echo "       Expected: libgodot-cpp.ios.${TARGET}.${ARCH}*.a"
        echo "       Available: $(ls "$SCRIPT_DIR/thirdparty/godot-cpp/bin/"*.a 2>/dev/null)"
        exit 1
    fi
    echo "  godot-cpp: $GODOT_CPP_LIB"

    # Merge extension .a + godot-cpp .a + ThorVG .a into one archive.
    # All three are needed: extension code, C++ bindings, and the renderer.
    local THORVG_LIB="$THORVG_BUILDDIR/src/libthorvg.a"
    if [ ! -f "$THORVG_LIB" ]; then
        echo "ERROR: ThorVG .a not found: $THORVG_LIB"
        echo "       Run: ./build_thorvg_ios.sh"
        exit 1
    fi
    libtool -static -o "$INTERMEDIATES/$OUT_NAME" "$PRODUCED" "$GODOT_CPP_LIB" "$THORVG_LIB"
    rm -f "$PRODUCED"
    echo "  -> $INTERMEDIATES/$OUT_NAME"
}

for tgt in $TARGETS; do
    for v in $VARIANTS; do
        build_variant "$tgt" "$v"
    done
done

# ---- Lipo simulator .a files (arm64 + x86_64) ----------------------------

combine_simulator_libs() {
    local TARGET="$1"
    local ARM_LIB="$INTERMEDIATES/libgodot_lottie.ios.$TARGET.arm64.simulator.a"
    local X86_LIB="$INTERMEDIATES/libgodot_lottie.ios.$TARGET.x86_64.simulator.a"
    local OUT="$INTERMEDIATES/libgodot_lottie.ios.$TARGET.simulator.a"

    if [ -f "$ARM_LIB" ] && [ -f "$X86_LIB" ]; then
        echo
        echo "=== lipo simulator .a files for $TARGET ==="
        lipo -create "$ARM_LIB" "$X86_LIB" -output "$OUT"
    elif [ -f "$ARM_LIB" ]; then
        cp "$ARM_LIB" "$OUT"
    elif [ -f "$X86_LIB" ]; then
        cp "$X86_LIB" "$OUT"
    fi
    [ -f "$OUT" ] && echo "  -> $OUT"
}

# ---- Build xcframeworks from static .a files -----------------------------

build_xcframework() {
    local TARGET="$1"
    local DEVICE_A="$INTERMEDIATES/libgodot_lottie.ios.$TARGET.arm64.a"
    local SIM_A="$INTERMEDIATES/libgodot_lottie.ios.$TARGET.simulator.a"
    local XCF="$BIN_DIR/libgodot_lottie.ios.$TARGET.xcframework"

    local ARGS=()
    if [ -f "$DEVICE_A" ]; then
        ARGS+=(-library "$DEVICE_A")
    fi
    if [ -f "$SIM_A" ]; then
        ARGS+=(-library "$SIM_A")
    fi
    if [ ${#ARGS[@]} -eq 0 ]; then
        echo "WARN: no .a files for $TARGET, skipping xcframework"
        return
    fi

    echo
    echo "=== xcodebuild -create-xcframework ($TARGET, static) ==="
    rm -rf "$XCF"
    xcodebuild -create-xcframework "${ARGS[@]}" -output "$XCF"
    echo "  -> $XCF"
}

for tgt in $TARGETS; do
    combine_simulator_libs "$tgt"
    build_xcframework "$tgt"
done

echo
echo "=== iOS extension build complete ==="
for tgt in $TARGETS; do
    XCF="$BIN_DIR/libgodot_lottie.ios.$tgt.xcframework"
    if [ -d "$XCF" ]; then
        echo "  $tgt: $XCF"
    fi
done
echo
echo "Test by exporting the demo project (or any project using the addon)"
echo "to iOS from Godot. The .xcframework will be linked into the generated"
echo "Xcode project automatically."
