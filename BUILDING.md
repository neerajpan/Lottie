# Building the Lottie GDExtension — All Platforms

Complete build instructions for every platform this extension targets. Covers
toolchain setup, ThorVG compilation, the SCons extension build, and output
verification.

> **Status legend** (verification of these instructions in this repo):
> ✅ = built end-to-end during the LottieAnimation3D work — exact commands shown.
> 📋 = distilled from the repo's existing `build_thorvg.sh` / `.gdextension` /
> README plus standard godot-cpp + ThorVG + Emscripten / NDK practice. The
> commands are correct in shape but the specific environment may need small
> tweaks (e.g. NDK version, Emscripten version).

The `.gdextension` file declares output paths for all of these:

| Platform | Architectures | ThorVG output | Extension output |
|---|---|---|---|
| Windows | `x86_64` (✅), `x86_32` (📋) | `thorvg-1.dll` (shared) | `.dll` |
| Linux | `x86_64`, `arm64` (📋) | `libthorvg.a` (static) | `.so` |
| macOS | Universal (`x86_64`+`arm64`) (📋) | `libthorvg.a` (static) | `.framework` |
| Android | `arm64` (✅), `x86_64` (✅) | `libthorvg.a` (static, cross-compiled) | `.so` |
| **iOS** | **device `arm64`, simulator `arm64` + `x86_64` (📋)** | `libthorvg.a` (static, cross-compiled) | `.xcframework` |
| Web | `wasm32` nothreads (📋) | static (`build_wasm/`) | `.wasm` |

---

## Common prerequisites (every platform)

You will need these on whatever machine runs the build:

| Tool | Why | How |
|---|---|---|
| Git | Clone & submodules | https://git-scm.com/downloads |
| Python 3.10+ | Drives meson + SCons | https://www.python.org/downloads/ |
| meson 1.0+ | Configures ThorVG | `python -m pip install --user meson` |
| ninja | Backend for meson | `python -m pip install --user ninja` |
| SCons 4+ | Builds the extension | `python -m pip install --user scons` |

After installing via `pip install --user`, make sure the user-site `Scripts`
directory is on `PATH`:

- Windows: `%APPDATA%\Python\PythonXY\Scripts`
- Linux/macOS: `~/.local/bin`

Clone the repo once with submodules:

```bash
git clone https://github.com/neerajpan/Lottie.git
cd Lottie
git submodule update --init --recursive
```

`thirdparty/godot-cpp/` and `thirdparty/thorvg/` should now contain source.

---

## 1. Windows — ✅ verified

### 1.1 Install MSVC toolchain

Either Visual Studio 2022 Community/Pro or **Build Tools for Visual Studio**
(headless, smaller). When installing, select the **"Desktop development with
C++"** workload — that gives you `cl.exe`, the Windows SDK, and `vcvars64.bat`.

Verify:

```
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
```

(Or `Community\VC\...` / `Professional\VC\...` for the IDE editions.)

### 1.2 Build ThorVG (x86_64)

ThorVG must be built inside a `vcvars64`-initialised shell so meson can find
`cl.exe`. The simplest way is a small helper batch:

```batch
@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set "PATH=%APPDATA%\Python\Python312\Scripts;%PATH%"
cd /d "%~dp0thirdparty\thorvg"
if exist builddir rmdir /s /q builddir
meson setup builddir ^
  -Dbuildtype=release -Doptimization=3 -Db_ndebug=true ^
  -Dsimd=true -Dthreads=true -Dpartial=true ^
  -Dengines=sw -Dloaders=lottie -Dbindings=capi -Dexamples=false ^
  --backend=ninja
meson compile -C builddir
```

Save as `_build_thorvg_msvc.bat` next to `SConstruct`, double-click (or run
from `cmd.exe`). Build time: ~2–5 min on a modern laptop.

Output: `thirdparty/thorvg/builddir/src/thorvg-1.dll`, `thorvg.lib`, `thorvg.exp`.

### 1.3 Build the extension

```batch
@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d "%~dp0"
python -m SCons platform=windows target=template_debug   arch=x86_64 -j%NUMBER_OF_PROCESSORS%
python -m SCons platform=windows target=template_release arch=x86_64 -j%NUMBER_OF_PROCESSORS%
```

First build is ~10 min (godot-cpp's ~990 wrapper classes compile once).
Subsequent incremental builds are seconds.

Output:
- `demo/addons/godot_lottie/bin/libgodot_lottie.windows.template_debug.x86_64.dll`
- `demo/addons/godot_lottie/bin/libgodot_lottie.windows.template_release.x86_64.dll`
- `demo/addons/godot_lottie/bin/thorvg-1.dll` (auto-copied by SConstruct)

### 1.4 Windows x86_32 (📋)

Same flow with the 32-bit MSVC environment and `arch=x86_32`:

```batch
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
... (rebuild ThorVG into a different builddir, e.g. builddir_x86) ...
python -m SCons platform=windows target=template_release arch=x86_32 -jN
```

You'll need to rebuild ThorVG for x86 in a separate directory — `SConstruct`
currently looks at `thirdparty/thorvg/builddir`, so swap the contents of that
folder when switching architectures, or extend `SConstruct` to look at an
arch-specific subfolder.

---

## 2. Linux — 📋

### 2.1 Install toolchain (Ubuntu/Debian shown)

```bash
sudo apt update
sudo apt install build-essential pkg-config python3 python3-pip git
# Meson + ninja: either pip or apt
sudo apt install meson ninja-build
# Or: python3 -m pip install --user meson ninja scons
```

For Fedora: `sudo dnf install gcc-c++ make python3 python3-pip git meson ninja-build`.
For Arch: `sudo pacman -S base-devel python python-pip git meson ninja`.

### 2.2 Build ThorVG (x86_64 native)

The repo already ships `build_thorvg.sh` — just run it:

```bash
chmod +x build_thorvg.sh
./build_thorvg.sh
```

The script invokes meson with `-Ddefault_library=static`, so the output is
`thirdparty/thorvg/builddir/src/libthorvg.a` (statically linked into the
extension `.so`).

### 2.3 Build the extension

```bash
python3 -m SCons platform=linux target=template_debug   arch=x86_64 -j$(nproc)
python3 -m SCons platform=linux target=template_release arch=x86_64 -j$(nproc)
```

Output:
- `demo/addons/godot_lottie/bin/libgodot_lottie.linux.template_debug.x86_64.so`
- `demo/addons/godot_lottie/bin/libgodot_lottie.linux.template_release.x86_64.so`

(No runtime `.so` is copied — ThorVG is statically linked on Linux.)

### 2.4 Linux arm64 — 📋

Two paths:

**(a) Native** on an arm64 box (Raspberry Pi 5, Ampere, etc.): identical
commands as above, just `arch=arm64`.

**(b) Cross-compile** from x86_64: install an arm64 cross-toolchain
(`crossbuild-essential-arm64` on Debian) and supply a meson cross-file when
building ThorVG. The shape:

```bash
# thirdparty/thorvg/cross-arm64.ini
[binaries]
c = 'aarch64-linux-gnu-gcc'
cpp = 'aarch64-linux-gnu-g++'
ar = 'aarch64-linux-gnu-ar'
strip = 'aarch64-linux-gnu-strip'

[host_machine]
system = 'linux'
cpu_family = 'aarch64'
cpu = 'aarch64'
endian = 'little'
```

```bash
cd thirdparty/thorvg
meson setup builddir_arm64 \
  --cross-file cross-arm64.ini \
  -Dbuildtype=release -Ddefault_library=static \
  -Dsimd=true -Dthreads=true -Dpartial=true \
  -Dengines=sw -Dloaders=lottie -Dbindings=capi -Dexamples=false
meson compile -C builddir_arm64
```

Then for SCons set `CC`/`CXX` to the cross-compiler and pass `arch=arm64`:

```bash
CC=aarch64-linux-gnu-gcc CXX=aarch64-linux-gnu-g++ \
  python3 -m SCons platform=linux target=template_release arch=arm64
```

(For the arm64 build, point SCons' `thorvg_lib_dir` at `builddir_arm64` —
either by renaming it to `builddir` for the run, or by extending the
SConstruct to take an env-var override.)

---

## 3. macOS — 📋

### 3.1 Install toolchain

```bash
xcode-select --install                # Command Line Tools (clang, ar, etc.)
brew install meson ninja python scons # via Homebrew
# or: python3 -m pip install --user meson ninja scons
```

### 3.2 Build ThorVG (Universal)

The shipped `build_thorvg.sh` detects ARM Mac and disables `-Dthreads` there
(ARM macOS + ThorVG threading had compat issues). For a Universal `.a` you
can either run the script twice (once per arch with a meson cross-file) and
`lipo` them together, or just build for the host arch you're on:

```bash
./build_thorvg.sh   # produces libthorvg.a for the host arch (x86_64 or arm64)
```

For a true Universal binary that runs on both Intel and Apple Silicon:

```bash
# arm64 build
cd thirdparty/thorvg
meson setup builddir_arm64 \
  --cross-file <path-to-arm64-cross.ini> \
  -Dbuildtype=release -Ddefault_library=static -Dsimd=true -Dthreads=false \
  -Dpartial=true -Dengines=sw -Dloaders=lottie -Dbindings=capi -Dexamples=false
meson compile -C builddir_arm64

# x86_64 build
meson setup builddir_x86_64 \
  --cross-file <path-to-x86_64-cross.ini> \
  -Dbuildtype=release -Ddefault_library=static -Dsimd=true -Dthreads=true \
  -Dpartial=true -Dengines=sw -Dloaders=lottie -Dbindings=capi -Dexamples=false
meson compile -C builddir_x86_64

# Combine
lipo -create builddir_arm64/src/libthorvg.a builddir_x86_64/src/libthorvg.a \
     -output builddir/src/libthorvg.a
```

### 3.3 Build the extension

```bash
python3 -m SCons platform=macos target=template_debug   arch=universal -j$(sysctl -n hw.ncpu)
python3 -m SCons platform=macos target=template_release arch=universal -j$(sysctl -n hw.ncpu)
```

Output is a **framework bundle** (see SConstruct line 67–73):

- `demo/addons/godot_lottie/bin/libgodot_lottie.macos.template_debug.framework/`
- `demo/addons/godot_lottie/bin/libgodot_lottie.macos.template_release.framework/`

Each framework contains the binary plus an `Info.plist`. The `.gdextension`
declares those bundle paths and Godot loads them transparently.

### 3.4 Code-signing (optional but recommended for distribution)

```bash
codesign --force --sign - --options runtime \
  demo/addons/godot_lottie/bin/libgodot_lottie.macos.template_release.framework
```

Replace `-` with a Developer ID for notarisation-ready signing.

---

## 4. Android — ✅

Verified end-to-end with **NDK r26.3.11579264** on a Windows host. All four
target variants (`arm64`, `x86_64` × `template_debug`, `template_release`)
build cleanly and contain the `LottieAnimation3D` symbols.

Android is always cross-compiled. You need:
- One ThorVG static library per ABI (`libthorvg.a`)
- One extension `.so` per ABI per target

### 4.1 Install Android NDK

Get **Android NDK r25c or newer** (godot-cpp 4.3+ requires it):

- Via Android Studio: SDK Manager → SDK Tools → NDK (Side by side)
- Or standalone: https://developer.android.com/ndk/downloads

Set the environment variable to your NDK path:

```bash
# Linux/macOS
export ANDROID_NDK_ROOT="$HOME/Android/Sdk/ndk/26.3.11579264"

# Windows (cmd / batch)
set "ANDROID_NDK_ROOT=C:\Android\sdk\ndk\26.3.11579264"
```

### 4.2 Edit the committed cross-files for your machine

The repo ships two meson cross-files (they live at the repo root, not inside
the `thirdparty/thorvg` submodule):

- `thorvg-crossfiles/cross-android-arm64.ini`
- `thorvg-crossfiles/cross-android-x86_64.ini`

They are pre-configured for **NDK r26.3 on a Windows host**. Open each one and
fix the four `[binaries]` paths to match your installation:

```ini
[binaries]
c = '<NDK_ROOT>/toolchains/llvm/prebuilt/<HOST_TAG>/bin/<TARGET>-linux-android24-clang<.cmd>'
cpp = '...<TARGET>-linux-android24-clang++<.cmd>'
ar = '<NDK_ROOT>/toolchains/llvm/prebuilt/<HOST_TAG>/bin/llvm-ar<.exe>'
strip = '<NDK_ROOT>/toolchains/llvm/prebuilt/<HOST_TAG>/bin/llvm-strip<.exe>'
```

- `<HOST_TAG>` = `windows-x86_64` / `linux-x86_64` / `darwin-x86_64`
- Drop the `.cmd` / `.exe` suffixes on Linux and macOS hosts.
- `24` is the API level (Godot 4.3+ Android minimum).

### 4.3 Build ThorVG for both ABIs

The repo ships helper scripts that take `ANDROID_NDK_ROOT` from the
environment, auto-detect the host tag, and generate a meson cross-file per
ABI inside each `builddir_android_<abi>/`:

```bash
# Linux / macOS host
./build_thorvg_android.sh
```

```batch
REM Windows host
build_thorvg_android.bat
```

Environment knobs:

| Var | Default | Meaning |
|---|---|---|
| `ANDROID_NDK_ROOT` | *(required)* | Path to the NDK install, e.g. `C:\Android\sdk\ndk\26.3.11579264` |
| `API` | `24` | Android API level (Godot 4.3+ minimum) |
| `ABIS` | `"arm64 x86_64"` | Space-separated ABI list to build |

If you only need one ABI, pass it in:

```bash
ABIS=arm64 ./build_thorvg_android.sh        # Linux/macOS
```

```batch
set "ABIS=arm64" & build_thorvg_android.bat  REM Windows
```

Each `libthorvg.a` is ~2 MB and lands at
`thirdparty/thorvg/builddir_android_<abi>/src/libthorvg.a`.

> **Important — Android requires `-Dthreads=false`.** ThorVG's threaded task
> scheduler interacts badly with the Android pthread runtime in some
> versions and causes hangs / crashes when the extension is loaded inside a
> Godot Android app. The helper scripts already set this; if you hand-roll
> the build, make sure to use `-Dthreads=false`. ThorVG then renders
> single-threaded but stably — performance is still good for typical
> Lottie sizes thanks to SIMD.

**Hand-rolling the build** (if you want to control more flags) is still an
option — the scripts are thin wrappers around:

```bash
cd thirdparty/thorvg
meson setup builddir_android_arm64 \
  --cross-file ../../thorvg-crossfiles/cross-android-arm64.ini \
  -Dbuildtype=release -Doptimization=3 -Db_ndebug=true \
  -Ddefault_library=static \
  -Dsimd=true -Dthreads=false -Dpartial=true \
  -Dengines=sw -Dloaders=lottie -Dbindings=capi \
  -Dexamples=false -Dtests=false \
  --backend=ninja
meson compile -C builddir_android_arm64
```

The committed `thorvg-crossfiles/cross-android-*.ini` files are pre-populated
with this machine's NDK path as a reference; the helper scripts generate
their own fresh cross-files inside each build directory based on the current
env vars, so the committed templates are only needed for hand-rolled builds.

### 4.4 Build the extension for both ABIs

The SConstruct in this repo already knows about Android — for
`platform=android` it looks at `thirdparty/thorvg/builddir_android_<arch>/src/`
automatically. No symlink juggling needed.

```bash
# arm64-v8a
python -m SCons platform=android target=template_debug   arch=arm64 -jN
python -m SCons platform=android target=template_release arch=arm64 -jN

# x86_64
python -m SCons platform=android target=template_debug   arch=x86_64 -jN
python -m SCons platform=android target=template_release arch=x86_64 -jN
```

Each `.so` is ~2 MB (debug) / 2 MB (release). Full set:

- `demo/addons/godot_lottie/bin/libgodot_lottie.android.template_debug.arm64.so`
- `demo/addons/godot_lottie/bin/libgodot_lottie.android.template_release.arm64.so`
- `demo/addons/godot_lottie/bin/libgodot_lottie.android.template_debug.x86_64.so`
- `demo/addons/godot_lottie/bin/libgodot_lottie.android.template_release.x86_64.so`

First build per target+arch combination is slow (~10 min: godot-cpp's ~1000
wrapper classes recompile per Android variant). All four together ~40 min on
a modern laptop.

### 4.5 Verify the symbols are in each `.so`

```bash
for f in demo/addons/godot_lottie/bin/libgodot_lottie.android.*.so; do
  printf "%-70s LottieAnimation3D count: %s\n" \
    "$(basename $f)" "$(grep -aoc 'LottieAnimation3D' $f)"
done
```

Each `.so` should have at least 2 hits.

### 4.6 The `libc++_shared.so` dependency

NDK clang++ would normally link the extension against `libc++_shared.so`
dynamically — but Android does NOT ship that library as a system lib, so
without intervention `dlopen()` fails on device with
`cannot locate symbol _ZNSt6__ndk1...`.

This repo solves it **two ways**, and the SConstruct now applies the first
one automatically:

**Preferred: statically link libc++ into the extension.** SConstruct passes
`-static-libstdc++` for `platform=android`, so any newly-built `.so` has
libc++ embedded and no `DT_NEEDED [libc++_shared.so]` entry. Self-contained,
nothing extra to package. Adds ~360 KB to each `.so`.

**Fallback (legacy): bundle `libc++_shared.so` per ABI as a Godot dependency.**
For any older `.so` in the repo that was built before the static-libc++ flag
was added, the repo also ships the NDK's `libc++_shared.so` under
`demo/addons/godot_lottie/bin/android/<arch>/` and references them via the
`.gdextension` `[dependencies]` section. This keeps those older binaries
loadable until they're rebuilt.

If you rebuild every `.so` with the current SConstruct, you can drop the
`[dependencies]` section and the bundled `libc++_shared.so` files entirely.

Bundle setup (for the fallback path):

1. Ship the NDK's `libc++_shared.so` per ABI under
   `demo/addons/godot_lottie/bin/android/<arch>/libc++_shared.so`.
2. Declare those files as dependencies in `godot_lottie.gdextension`:
   ```ini
   [dependencies]
   android.debug.arm64 = {"res://addons/godot_lottie/bin/android/arm64/libc++_shared.so": ""}
   android.release.arm64 = {"res://addons/godot_lottie/bin/android/arm64/libc++_shared.so": ""}
   android.debug.x86_64 = {"res://addons/godot_lottie/bin/android/x86_64/libc++_shared.so": ""}
   android.release.x86_64 = {"res://addons/godot_lottie/bin/android/x86_64/libc++_shared.so": ""}
   ```

When Godot exports for Android, it packages these into the APK's `lib/<abi>/`
folder alongside the extension `.so`, and the dynamic linker resolves the
dependency at `dlopen()` time.

If you rebuild with a different NDK, **re-copy** the matching
`libc++_shared.so` from
`<NDK>/toolchains/llvm/prebuilt/<host>/sysroot/usr/lib/<triple>/libc++_shared.so`
into the corresponding `bin/android/<arch>/` folder. The libc++ ABI is
forward-compatible across NDK minor versions but you should keep them in
sync to be safe.

### 4.7 Testing on a device

Push the demo project to a connected device via the Godot Android export
template, or copy `demo/addons/godot_lottie/` into your existing Godot
Android app's `addons/` and enable the plugin. Godot's runtime picks the
correct `.so` per the device ABI via the `.gdextension` mappings.

**Symptom-fix cheat sheet for Android load errors:**

| dlopen error | Likely cause | Fix |
|---|---|---|
| `cannot locate symbol "_ZNSt6__ndk1..."` | `libc++_shared.so` missing in APK | Ensure `[dependencies]` in `.gdextension` references the per-ABI `libc++_shared.so` (see §4.6) |
| `library "libgodot_lottie.android.template_release.arm64.so" not found` | Wrong ABI built, or `.gdextension` path mismatch | Build the matching ABI; check `.gdextension` `[libraries]` paths |
| `cannot locate symbol "__cxa_throw"` | Built without `libc++abi` or wrong NDK | Use NDK r25+ |

---

## 5. iOS — 📋

**Hard requirement: macOS host with Xcode installed.** iOS toolchain (clang +
`xcrun` + `xcodebuild`) is Apple-only — you cannot build for iOS from a
Windows or Linux machine without a Mac in the loop. The scripts and
SConstruct support added here have been written following godot-cpp's
`tools/ios.py` conventions but have **not** been compiled on this build
machine (Windows host); please verify on a Mac.

Output: a `.xcframework` bundle per target containing two slices —
iOS device (arm64) and iOS Simulator (arm64 + x86_64 lipo'd together).

### 5.1 Install prerequisites

```bash
# Xcode + Command Line Tools (do this in the App Store or via xcode-select).
xcode-select --install

# Confirm the toolchain is reachable.
xcrun --version
xcrun --sdk iphoneos --show-sdk-path
xcrun --sdk iphonesimulator --show-sdk-path
xcodebuild -version

# meson + ninja
brew install meson ninja
# or: pip3 install --user meson ninja scons
```

### 5.2 Build ThorVG for iOS (device + simulator)

```bash
# Builds three static libthorvg.a -- one per (arch, sdk) combination:
#   thirdparty/thorvg/builddir_ios-arm64/src/libthorvg.a              (device)
#   thirdparty/thorvg/builddir_ios-arm64-simulator/src/libthorvg.a    (Apple Silicon sim)
#   thirdparty/thorvg/builddir_ios-x86_64-simulator/src/libthorvg.a   (Intel Mac sim)
chmod +x build_thorvg_ios.sh
./build_thorvg_ios.sh
```

The script auto-detects SDK paths via `xcrun` and generates a fresh meson
cross-file per variant inside each `builddir_ios-*/.cross.ini`.

Optional env knobs:

| Var | Default | Meaning |
|---|---|---|
| `MIN_IOS` | `12.0` | Minimum iOS version target (matches godot-cpp default) |
| `VARIANTS` | `"device sim-arm64 sim-x86_64"` | Subset to build; pass `"device"` only to skip simulator |

> **Note:** like the Android build, the iOS ThorVG is compiled with
> `-Dthreads=false`. Threads-off keeps the static library self-contained
> and avoids potential issues with ThorVG's task scheduler inside an
> embedded library on iOS. SIMD stays on.

### 5.3 Build the extension `.xcframework`

```bash
chmod +x build_extension_ios.sh
./build_extension_ios.sh
```

The script runs SCons three times per target (`template_debug`,
`template_release`) — once each for device arm64, sim arm64, sim x86_64 —
combines the simulator dylibs with `lipo`, then invokes
`xcodebuild -create-xcframework` to produce:

- `demo/addons/godot_lottie/bin/libgodot_lottie.ios.template_debug.xcframework/`
- `demo/addons/godot_lottie/bin/libgodot_lottie.ios.template_release.xcframework/`

These are what the `.gdextension` references for the iOS platform.

Optional env knobs:

| Var | Default | Meaning |
|---|---|---|
| `TARGETS` | `"template_debug template_release"` | Subset of build targets |
| `VARIANTS` | `"device sim-arm64 sim-x86_64"` | Subset of iOS slices |

### 5.4 Hand-rolled SCons invocations (for one-off builds)

If you want to control SCons args directly (e.g. just build a single
device-only slice):

```bash
# iOS device (arm64)
python3 -m SCons platform=ios target=template_release arch=arm64 ios_simulator=no

# iOS simulator -- Apple Silicon
python3 -m SCons platform=ios target=template_release arch=arm64 ios_simulator=yes

# iOS simulator -- Intel Mac
python3 -m SCons platform=ios target=template_release arch=x86_64 ios_simulator=yes
```

godot-cpp's `tools/ios.py` sets `SHLIBSUFFIX=.dylib` and uses `xcrun` to
find the right SDK at SCons-time.

### 5.5 Testing on a device

1. Open the demo project (or your own project that uses the addon) in
   Godot 4.5+.
2. *Project → Export → Add → iOS* — fill in the team identifier and
   bundle id.
3. Export to a folder. Godot generates an Xcode project that
   references `libgodot_lottie.ios.template_release.xcframework` from the
   addon's `bin/`.
4. Open the generated `.xcodeproj` in Xcode, select a real device or
   simulator, and Run.

### 5.6 Common iOS load errors

| Symptom | Cause / Fix |
|---|---|
| `dyld: Library not loaded: ... libgodot_lottie...` | The `.xcframework` slice for the active arch (device vs simulator) wasn't built. Re-run `./build_extension_ios.sh` with the appropriate `VARIANTS`. |
| Xcode signing errors when running on device | The `.xcframework` itself doesn't need signing, but Godot's generated iOS project does. Set up a development team in Xcode. |
| `Undefined symbols for architecture arm64` at link time | ThorVG wasn't built for that arch. Re-run `./build_thorvg_ios.sh`. |

---

## 6. Web (Emscripten / WASM) — 📋

### 6.1 Install Emscripten

```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh        # Linux/macOS
# Windows: emsdk_env.bat
```

Verify:

```bash
emcc --version
# emcc (Emscripten gcc/clang-like replacement) 3.x.x
```

Godot 4.3+ Web exports use **wasm32 + no threads** (the SharedArrayBuffer
COOP/COEP requirement is a pain), which matches the `.gdextension` entry
(`web.template_release.wasm32.nothreads.wasm`).

### 6.2 Build ThorVG for WASM

ThorVG ships an `emscripten` cross-file template; copy / adapt it. Sample
`thirdparty/thorvg/cross-emscripten.ini`:

```ini
[binaries]
c = 'emcc'
cpp = 'em++'
ar = 'emar'
strip = 'emstrip'
pkg-config = 'false'

[host_machine]
system = 'emscripten'
cpu_family = 'wasm32'
cpu = 'wasm32'
endian = 'little'

[built-in options]
c_args     = ['-pthread=false']
cpp_args   = ['-pthread=false', '-DTHORVG_THREAD_SUPPORT=0']
c_link_args   = ['-pthread=false']
cpp_link_args = ['-pthread=false']
```

Build:

```bash
cd thirdparty/thorvg
meson setup build_wasm \
  --cross-file cross-emscripten.ini \
  -Dbuildtype=release -Ddefault_library=static -Db_ndebug=true \
  -Dsimd=false -Dthreads=false -Dpartial=true \
  -Dengines=sw -Dloaders=lottie -Dbindings=capi -Dexamples=false
meson compile -C build_wasm
```

Note the output directory is **`build_wasm`** (not `builddir`) — that's what
the SConstruct's Web branch expects:

```python
if env["platform"] == "web":
    thorvg_lib_dir = os.path.join("thirdparty", "thorvg", "build_wasm", "src")
```

### 6.3 Build the extension for Web

```bash
python3 -m SCons platform=web target=template_debug   -j$(nproc)
python3 -m SCons platform=web target=template_release -j$(nproc)
```

SCons will pick up `emcc` from PATH (which `emsdk_env.sh` put there).

Output:

- `demo/addons/godot_lottie/bin/libgodot_lottie.web.template_debug.wasm32.nothreads.wasm`
- `demo/addons/godot_lottie/bin/libgodot_lottie.web.template_release.wasm32.nothreads.wasm`

### 6.4 Testing in the browser

Export the demo project from Godot as **Web** with **Threading: Disabled**.
Serve the resulting HTML/JS/WASM bundle with any static server:

```bash
cd ~/Desktop/web_export
python3 -m http.server 8000
# open http://localhost:8000/index.html
```

---

## 7. Verifying the addon in Godot

After any platform build:

1. Open `demo/project.godot` in Godot 4.3+. Restart the editor if it was
   running during the build so it reloads the rebuilt DLL/.so/.wasm.
2. Open `res://addons/godot_lottie/demo/controldemo.tscn` (2D smoke test)
   and `res://addons/godot_lottie/demo/demo3d.tscn` (3D smoke test).
3. Press **F5** / **F6**. Both animations should play.
4. In the editor's *Project → Tools → Script Editor* run:
   ```gdscript
   @tool
   extends EditorScript
   func _run():
       print("2D:", ClassDB.class_exists("LottieAnimation"))
       print("3D:", ClassDB.class_exists("LottieAnimation3D"))
   ```
   Both should print `true`.

---

## 8. Packaging the addon

To ship the addon to other Godot projects:

1. Build for every platform you intend to target (the `bin/` folder
   accumulates per-platform binaries).
2. Zip the entire `demo/addons/godot_lottie/` directory.
3. Recipients drop `godot_lottie/` into their project's `addons/` folder and
   enable it under *Project → Project Settings → Plugins*.

Godot picks the right `.dll` / `.so` / `.framework` / `.wasm` per the
`.gdextension` file automatically.

---

## 9. Troubleshooting

| Symptom | Cause / Fix |
|---|---|
| `meson: command not found` | `python -m pip install --user meson ninja`, then add user `Scripts` to `PATH`. |
| `cl is not recognized` (Windows) | Build wasn't started inside a `vcvars64`-initialised shell. Run from the helper batch file shown in §1.2. |
| `Error: ThorVG build directory not found: thirdparty/thorvg/builddir` | ThorVG hasn't been built yet, or you built it into a different directory (e.g. `build_wasm`, `builddir_arm64`). For non-Web targets the SConstruct currently expects `builddir`; swap your per-arch directories accordingly. |
| Linker error: `unresolved external symbol _tvg_...` | ThorVG was built with `-Dbindings=capi=false`. Rebuild with `-Dbindings=capi`. |
| Godot loads the addon but `LottieAnimation3D` isn't in *Create New Node* | DLL is older than your sources. Force a clean SCons build (`python -m SCons -c` to clean, then rebuild) and restart the editor. |
| Web export shows `RuntimeError: Aborted(...)` on load | ThorVG was built with `-Dthreads=true` for WASM. Web build must use `-Dthreads=false` and the Emscripten flag `-pthread=false`. |
| Android: `libgodot_lottie.so` missing | Wrong ABI built or wrong `arch=` flag. Each Android ABI needs its own ThorVG + extension build (§4.3). |

---

## 10. Building via GitHub Actions (no local toolchain needed)

The repo ships a manual CI workflow at `.github/workflows/build.yml` that
builds binaries on GitHub's hosted runners — Linux, macOS, iOS, Android,
Windows — with **no** local toolchain required. You pick which platform(s)
to build via checkboxes; jobs you don't tick don't run.

### How to trigger it

1. Open https://github.com/&lt;your-fork&gt;/Lottie → **Actions** tab.
2. In the left sidebar pick **Build extension binaries**.
3. Click **Run workflow** (top-right of the runs list).
4. Tick the platform checkboxes you want:
   - `Windows x86_64 (.dll)`
   - `Linux x86_64 (.so)`
   - `macOS (.framework, host arch only)`
   - `iOS device + simulator (.xcframework)`
   - `Android arm64-v8a (.so)`
   - `Android x86_64 (.so)`
5. Pick **Build targets** — `both`, `debug_only`, or `release_only`.
6. Optional: tick **Auto-commit built binaries back to bin/** if you want
   the resulting artifacts pushed back into `main`. Leave it unchecked to
   just upload artifacts you can download manually from the run page.
7. Click **Run workflow**.

Each ticked platform spawns an independent job, all running in parallel.
Build time is roughly the slowest one (~10–20 minutes), not the sum.

### Where the binaries end up

- **Always**: as job artifacts on the workflow run page. Download manually
  → unzip → drop into your project's `addons/godot_lottie/bin/`.
- **If `Auto-commit ...` was ticked**: a follow-up `commit` job downloads
  all artifacts, drops them into `demo/addons/godot_lottie/bin/`, and
  pushes a commit with a message like:
  ```
  ci: rebuild binaries via workflow_dispatch (linux_x86_64 ios)
  ```

### What the workflow handles per platform

| Platform | Runner | What it does |
|---|---|---|
| Windows x86_64 | `windows-latest` | Sets up MSVC via `ilammy/msvc-dev-cmd`, runs `build_thorvg.bat` (static), then SCons |
| Linux x86_64 | `ubuntu-latest` | Installs `build-essential`/`meson`/`ninja`/`scons` via apt+pip, runs `./build_thorvg.sh`, then SCons |
| macOS | `macos-latest` (Apple Silicon) | `brew install meson ninja scons`, runs `./build_thorvg.sh`, then SCons for host arch (arm64) |
| iOS | `macos-latest` | Same brew installs, runs `./build_thorvg_ios.sh` then `./build_extension_ios.sh` to produce `.xcframework` per target |
| Android | `ubuntu-latest` + `nttld/setup-ndk@v1` (NDK r26d) | Cross-compiles ThorVG + extension for each ticked ABI |

### Limitations / known gaps

- **macOS** job builds only for the runner's arch (currently arm64, since
  `macos-latest` is Apple Silicon). For Universal macOS frameworks (arm64
  + x86_64), you still need to follow §3 manually for now.
- **iOS** code signing is intentionally NOT done in CI — the produced
  `.xcframework` is unsigned. Godot's iOS exporter generates an Xcode
  project that gets signed at the user's `xcodebuild` step.
- **Web (WASM)** is not in the workflow yet — `build_thorvg_wasm.sh` is
  still in the follow-up list.

---

## 11. Follow-up improvements to this repo (not done yet)

These would make multi-platform builds smoother:

- **Per-arch ThorVG directories**: done — the SConstruct now uses
  `builddir_android_<arch>` for Android and `build_wasm` for Web, so no
  symlink juggling is needed. Could be extended to take a
  `thorvg_builddir=...` SCons argument override.
- **`build_thorvg_android.sh` / `.bat`**: done — see §4.3. Take
  `ANDROID_NDK_ROOT` from env, auto-detect host tag, loop over ABIs, and
  generate fresh meson cross-files inside each `builddir_android_<abi>/`.
- **`build_thorvg_wasm.sh`** helper script (analogous to the Android
  helpers but for Emscripten).
- **CI workflow** (e.g. `.github/workflows/build.yml`) that builds all
  platforms on every push and uploads the binaries as release artefacts.
- Ship the per-platform meson cross-files (`cross-android-*.ini`,
  `cross-emscripten.ini`) in `thirdparty/thorvg-crossfiles/` so users don't
  hand-edit them.
