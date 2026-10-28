# FlowtoysUpdater

Desktop app for updating the firmware on [flowtoys](https://flowtoys.com) props — **Capsule 2.0** and **Vision** (club) — over USB.

Plug in your prop, pick a firmware, press upload. The app finds props on USB, downloads the latest firmware images from flowtoys automatically, and can flash several props at once from a USB hub.

Runs on macOS (Apple Silicon and Intel), Windows (x64), and Linux.

## Download

- Latest builds: [GitHub Releases](https://github.com/flowtoys-inc/FlowtoysUpdater/releases/latest). Take the `.pkg` for macOS or the `.exe` for Windows.
- User instructions: [flowtoys product support](https://flowtoys.com/pages/product-support) → flowOS firmware update

The macOS app is signed and notarized. The Windows installer isn't code signed, so SmartScreen may say *Windows protected your PC*: choose *More info → Run anyway*. Where users find the updater is in [docs/DISTRIBUTION.md](docs/DISTRIBUTION.md).

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
| [docs/DISTRIBUTION.md](docs/DISTRIBUTION.md) | Where users download the app, which help articles to keep in sync |

## Contributing

Pull requests are welcome. The flowtoys-inc rules for every repo are in [CONTRIBUTING.md](https://github.com/flowtoys-inc/dev-standards/blob/main/CONTRIBUTING.md), with the full version for AI assistants in [CLAUDE.md](https://github.com/flowtoys-inc/dev-standards/blob/main/CLAUDE.md). In short:

- Branch off `master`, and give the PR a [Conventional Commit](https://www.conventionalcommits.org/) title such as `fix: keep the firmware list after a failed download`. PRs are squash-merged, and the title drives the version and changelog.
- Before pushing, format the code and build with warnings as errors:

  ```bash
  uvx clang-format@23.1.3 -i Source/*.cpp Source/*.h Tests/*.cpp
  cmake -B build -DCMAKE_BUILD_TYPE=Release -DFLOWTOYS_WARNINGS_AS_ERRORS=ON
  cmake --build build --parallel && ctest --test-dir build --output-on-failure
  ```

- Anything touching USB/HID or flashing needs testing on a real prop. Say in the PR what you flashed.

## License

FlowtoysUpdater is licensed under the [GPLv3](LICENSE). It is built on [JUCE](https://juce.com) (used under the AGPLv3, which GPLv3 permits combining with) and [hidapi](https://github.com/libusb/hidapi).

Originally written by [Ben Kuper](https://github.com/benkuper).
