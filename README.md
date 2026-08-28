# FlowtoysUpdater

Desktop app for updating the firmware on [flowtoys](https://flowtoys.com) props — **Capsule 2.0** and **Vision** (club) — over USB.

Plug in your prop, pick a firmware, press upload. The app finds props on USB, downloads the latest firmware images from flowtoys automatically, and can flash several props at once from a USB hub.

Runs on macOS (Apple Silicon and Intel), Windows (x64), and Linux.

## Download

- Latest builds: [GitHub Releases](https://github.com/flowtoys-inc/FlowtoysUpdater/releases)
- Official flowtoys download page: [flowtoys.com](https://www.flowtoys.com)

> **macOS note**: builds are not yet signed/notarized. On first launch, right-click the app and choose *Open* (or allow it under *System Settings → Privacy & Security*).

## Building from source

```bash
git clone --recurse-submodules https://github.com/flowtoys-inc/FlowtoysUpdater.git
cd FlowtoysUpdater
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

The app lands in `build/FlowtoysUpdater_artefacts/Release/`. Full per-platform instructions (dependencies, universal macOS builds, Linux udev rule, tests) are in [docs/BUILDING.md](docs/BUILDING.md).

## Documentation

| Document | Contents |
|---|---|
| [docs/BUILDING.md](docs/BUILDING.md) | Build prerequisites and commands per platform, running the tests |
| [docs/RELEASING.md](docs/RELEASING.md) | Release process: versioning, tagging, CI artifacts, signing, server-side steps |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | How the app is put together: screens, managers, threads |
| [docs/HID-PROTOCOL.md](docs/HID-PROTOCOL.md) | The USB HID bootloader protocol used to flash props |
| [docs/FIRMWARE-FORMAT.md](docs/FIRMWARE-FORMAT.md) | The `.fwimg` firmware file format and the server index |
| [docs/PLAN.md](docs/PLAN.md) | The 2026 modernization plan and roadmap |

## License

FlowtoysUpdater is licensed under the [GPLv3](LICENSE). It is built on [JUCE](https://juce.com) (used under the AGPLv3, which GPLv3 permits combining with) and [hidapi](https://github.com/libusb/hidapi).

Originally written by [Ben Kuper](https://github.com/benkuper).
