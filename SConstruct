#!/usr/bin/env python

"""
ThorVG-Godot Build Configuration
Builds optimized Lottie animation extension for Godot Engine
"""

import os
import sys
import shutil

env = SConscript("thirdparty/godot-cpp/SConstruct")

# Linux: enable OpenMP for ThorVG's threading support
# ThorVG is built with -Dthreads=true which uses OpenMP for parallel rendering
# Add here (after godot-cpp import) to avoid affecting godot-cpp's build
if env["platform"] == "linux":
    env.Append(CCFLAGS=["-fopenmp"])
    env.Append(LINKFLAGS=["-fopenmp"])

# Source files
env.Append(CPPPATH=["src/"])
sources = Glob("src/*.cpp")

# ThorVG integration - Platform-specific library handling
env.Append(CPPPATH=["thirdparty/thorvg/inc"])
# TVG_STATIC tells thorvg.h to skip the __declspec(dllimport) decoration on
# its public API symbols, which is required when linking against the static
# libthorvg.a / thorvg.lib (otherwise MSVC emits LNK4217 and refuses to
# link). Safe on non-Windows -- on Linux/macOS it just makes TVG_API empty,
# which is what those builds already use.
env.Append(CPPDEFINES=["TVG_STATIC"])

# Android: statically link libc++ into the extension so the .so does not
# require libc++_shared.so to be present alongside it at runtime. Otherwise
# the dynamic linker fails with `cannot locate symbol _ZNSt6__ndk1...` when
# Godot dlopen()s the extension on a device.
if env["platform"] == "android":
    env.Append(LINKFLAGS=["-static-libstdc++"])


# Determine ThorVG library location and linking method.
#
# Android, iOS and Web are cross-compiled and each ABI/target needs its own
# ThorVG build, so we look in arch-specific build directories for those.
# Native desktop builds use the canonical `builddir`.
if env["platform"] == "web":
    thorvg_lib_dir = os.path.join("thirdparty", "thorvg", "build_wasm", "src")
elif env["platform"] == "android":
    arch = env.get("arch", "arm64")
    thorvg_lib_dir = os.path.join("thirdparty", "thorvg", "builddir_android_" + arch, "src")
elif env["platform"] == "ios":
    # iOS has device (arm64) and simulator (arm64 / x86_64) builds.
    # build_thorvg_ios.sh produces:
    #   builddir_ios-arm64/                  (device)
    #   builddir_ios-arm64-simulator/        (Apple Silicon Mac simulator)
    #   builddir_ios-x86_64-simulator/       (Intel Mac simulator)
    arch = env.get("arch", "arm64")
    sim_suffix = "-simulator" if env.get("ios_simulator", False) else ""
    thorvg_lib_dir = os.path.join("thirdparty", "thorvg", "builddir_ios-" + arch + sim_suffix, "src")
else:
    thorvg_lib_dir = os.path.join("thirdparty", "thorvg", "builddir", "src")

if os.path.exists(thorvg_lib_dir):
    env.Append(LIBPATH=[thorvg_lib_dir])

    # Platform-specific library files
    platform_libs = {
        "windows": [
            ("thorvg.lib", lambda: env.Append(LIBS=["thorvg"])),
            ("libthorvg.a", lambda: env.Append(LINKFLAGS=[os.path.join(thorvg_lib_dir, "libthorvg.a")]))
        ],
        "linux": [
            ("libthorvg.a", lambda: env.Append(LIBS=["thorvg"])),
            ("libthorvg.so", lambda: env.Append(LIBS=["thorvg"]))
        ],
        "macos": [
            ("libthorvg.a", lambda: env.Append(LIBS=["thorvg"])),
            ("libthorvg.dylib", lambda: env.Append(LIBS=["thorvg"]))
        ],
        "android": [
            ("libthorvg.a", lambda: env.Append(LIBS=["thorvg"]))
        ],
        "ios": [
            ("libthorvg.a", lambda: env.Append(LIBS=["thorvg"]))
        ],
        "web": [
            ("libthorvg.a", lambda: env.Append(LIBS=["thorvg"]))
        ]
    }
    
    # Try to find and link appropriate library for current platform
    lib_found = False
    if env["platform"] in platform_libs:
        for lib_name, link_func in platform_libs[env["platform"]]:
            lib_path = os.path.join(thorvg_lib_dir, lib_name)
            if os.path.isfile(lib_path):
                link_func()
                lib_found = True
                print("Using ThorVG library: {}".format(lib_name))
                break
    
    if not lib_found:
        print("Warning: ThorVG library not found in {}".format(thorvg_lib_dir))
        print("Available files:", os.listdir(thorvg_lib_dir) if os.path.exists(thorvg_lib_dir) else "Directory not found")
else:
    print("Error: ThorVG build directory not found: {}".format(thorvg_lib_dir))
    print("Please run the appropriate build script first:")
    print("  Windows: build_thorvg.bat") 
    print("  Linux/macOS: ./build_thorvg.sh")

# Output library name
if env["platform"] == "macos":
    library = env.SharedLibrary(
        "demo/addons/godot_lottie/bin/libgodot_lottie.{}.{}.framework/libgodot_lottie.{}.{}".format(
            env["platform"], env["target"], env["platform"], env["target"]
        ),
        source=sources,
    )
else:
    library = env.SharedLibrary(
        "demo/addons/godot_lottie/bin/libgodot_lottie{}{}".format(env["suffix"], env["SHLIBSUFFIX"]),
        source=sources,
    )

Default(library)

# Copy ThorVG runtime DLL on Windows (Linux/macOS use static linking)
if env["platform"] == "windows":
    thorvg_runtime_dir = thorvg_lib_dir if 'thorvg_lib_dir' in locals() else os.path.join("thirdparty", "thorvg", "builddir", "src")
    thorvg_runtime = os.path.join(thorvg_runtime_dir, "thorvg-1.dll")
    dest_dir = os.path.join("demo", "addons", "godot_lottie", "bin")
    dest_dll = os.path.join(dest_dir, "thorvg-1.dll")

    if os.path.isfile(thorvg_runtime):
        def _copy_thorvg_dll(target, source, env):
            try:
                if not os.path.isdir(dest_dir):
                    os.makedirs(dest_dir, exist_ok=True)
                shutil.copy2(thorvg_runtime, dest_dll)
                print("Copied ThorVG runtime to {}".format(dest_dll))
            except Exception as e:
                print("Warning: Failed to copy ThorVG runtime DLL: {}".format(e))

        try:
            env.AddPostAction(library, _copy_thorvg_dll)
        except Exception:
            # Fallback: immediate copy
            try:
                if not os.path.isdir(dest_dir):
                    os.makedirs(dest_dir, exist_ok=True)
                shutil.copy2(thorvg_runtime, dest_dll)
                print("Copied ThorVG runtime to {} (fallback)".format(dest_dll))
            except Exception as e:
                print("Warning: Could not copy ThorVG runtime: {}".format(e))
