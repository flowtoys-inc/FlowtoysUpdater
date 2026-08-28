# Building FlowtoysUpdater

The build is CMake-based, using [JUCE](https://juce.com)'s native CMake support. JUCE and [hidapi](https://github.com/libusb/hidapi) are pinned git submodules under `ThirdParty/` — there is nothing else to install besides a toolchain (and some system packages on Linux).

## Get the source

```bash
git clone --recurse-submodules https://github.com/flowtoys-inc/FlowtoysUpdater.git
cd FlowtoysUpdater
```

Already cloned without submodules? Run `git submodule update --init`.

## macOS

Requires Xcode (or at minimum a toolchain that `xcodebuild`-independent CMake Makefile builds can use) and CMake ≥ 3.22.

```bash
# Universal binary (Apple Silicon + Intel), Release:
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --parallel
open build/FlowtoysUpdater_artefacts/Release/FlowtoysUpdater.app
```

Omit `-DCMAKE_OSX_ARCHITECTURES` for a faster native-only development build. The deployment target is 10.13 (set in `CMakeLists.txt`).

If `xcode-select -p` points at the CommandLineTools but you need a specific Xcode (e.g. a beta), prefix commands with `DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer` instead of switching system-wide.

## Windows

Requires Visual Studio 2022 (Community is fine) with the C++ workload, and CMake.

```powershell
cmake -B build
cmake --build build --config Release --parallel
```

The C runtime is statically linked (`CMAKE_MSVC_RUNTIME_LIBRARY` in `CMakeLists.txt`), so the produced `FlowtoysUpdater.exe` has no VC-redistributable dependency.

## Linux

Install the build dependencies (Debian/Ubuntu names):

```bash
sudo apt-get install -y build-essential cmake \
  libfreetype-dev libfontconfig1-dev \
  libx11-dev libxinerama-dev libxrandr-dev libxcursor-dev libxcomposite-dev libxext-dev \
  libcurl4-openssl-dev libasound2-dev \
  libusb-1.0-0-dev libudev-dev
```

Then:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/FlowtoysUpdater_artefacts/Release/FlowtoysUpdater
```

### udev rule (device access without root)

The app talks to props (USB VID `f107`) via hidapi's libusb backend. To use it as a regular user, install a udev rule:

```bash
echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="f107", MODE="0666"' | \
  sudo tee /etc/udev/rules.d/99-flowtoys.rules
sudo udevadm control --reload-rules && sudo udevadm trigger
```

## Running the tests

Unit tests use Catch2 (fetched automatically by CMake) and run through CTest:

```bash
cmake --build build --target FlowtoysUpdaterTests --parallel
ctest --test-dir build --output-on-failure
```

Pass `-DFLOWTOYS_BUILD_TESTS=OFF` at configure time to skip the test target entirely.

## Build options / conventions

- The app version lives in exactly one place: `project(FlowtoysUpdater VERSION x.y.z)` in the root `CMakeLists.txt`. Release tags must match it (CI enforces this — see [RELEASING.md](RELEASING.md)).
- Do **not** compile `Source/hid.c` / `Source/hid_osx.c` if you find them in an old checkout — the hidapi submodule replaces them.
- CI builds run on every PR (`.github/workflows/build.yml`): macOS universal, Windows, Linux.
