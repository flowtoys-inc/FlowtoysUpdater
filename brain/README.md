# FlowtoysUpdater documentation

Developer and agent docs for **FlowtoysUpdater**, the desktop app (JUCE 9, CMake, hidapi) that flashes
firmware onto flowtoys LED props (Capsule 2.0, Vision/Club) over USB HID. It downloads `.fwimg` firmware
from flow-toys.com, puts each prop into its bootloader and flashes all connected props in parallel. It
also updates itself.

> **This `brain/` tree is the map of the codebase.** When code and docs disagree, trust the code and fix
> the doc in the same PR. Agents: read the repo-root [`CLAUDE.md`](../CLAUDE.md) first (hard
> backward-compatibility constraints), then route with the [table below](#where-do-i-look-for).

---

## Document map

| Doc | What it's for |
|---|---|
| [`ARCHITECTURE.md`](ARCHITECTURE.md) | The five-screen wizard, the three singletons (`PropManager`, `FirmwareManager`, `AppUpdater`), threads and `QueuedNotifier`, repo layout. |
| [`HID-PROTOCOL.md`](HID-PROTOCOL.md) | USB VID/PIDs, 64-byte HID framing, bootloader commands and responses, hardware revisions, the per-prop flash sequence and failure behavior. **Frozen**: fielded bootloaders depend on it. |
| [`FIRMWARE-FORMAT.md`](FIRMWARE-FORMAT.md) | The `.fwimg` ZIP (`data` + `meta` JSON), the `firmwares.php` server index, the local cache. |
| [`BUILDING.md`](BUILDING.md) | Prerequisites and build commands for macOS, Windows and Linux, the udev rule, tests, warnings and formatting. |
| [`RELEASING.md`](RELEASING.md) | release-please flow, artifact naming contract, `update.json` schema, macOS signing and notarization, publishing to flow-toys.com, dependency updates. |
| [`DISTRIBUTION.md`](DISTRIBUTION.md) | Where users download the app, the help articles the app links to (edit in place, never delete), the Windows no-signing decision. |

---

## Where do I look for…?

| I want to… | Doc | Key source files |
|---|---|---|
| Understand the app flow / add or change a screen | [ARCHITECTURE](ARCHITECTURE.md) | `Source/ScreenManager.cpp`, `Source/Screen.h`, `Source/*Screen.cpp`, `Source/MainComponent.cpp` |
| Change device discovery or the auto-reset into bootloader | [HID-PROTOCOL](HID-PROTOCOL.md#device-identification) | `Source/PropManager.cpp`, `Source/PropConstants.h:28` (`productIds`, `flowtoysVID`) |
| Touch the flash state machine | [HID-PROTOCOL](HID-PROTOCOL.md#flash-sequence-per-prop-one-thread-each) | `Source/Prop.cpp` (erase sizes at `:312`), `Source/Prop.h` |
| Change hardware-revision compatibility rules | [HID-PROTOCOL](HID-PROTOCOL.md#hardware-revisions), [FIRMWARE-FORMAT](FIRMWARE-FORMAT.md) | `Source/FirmwareImage.h`, `Source/VersionUtils.h`, `Tests/FirmwareImageTests.cpp` |
| Parse `.fwimg` / change firmware download or cache | [FIRMWARE-FORMAT](FIRMWARE-FORMAT.md) | `Source/FirmwareImage.h`, `Source/FirmwareManager.cpp`, `Source/FirmwareManager.h:82` (`remoteHost`) |
| Change the self-updater | [RELEASING](RELEASING.md#updatejson-schema-what-the-client-actually-reads) | `Source/AppUpdater.cpp`, `Source/Main.cpp:41` (`update.json` URL) |
| Fix cross-thread UI updates | [ARCHITECTURE](ARCHITECTURE.md#threads-and-events) | `Source/QueuedNotifier.h` |
| Build, format, run tests | [BUILDING](BUILDING.md) | `CMakeLists.txt`, `Tests/CMakeLists.txt`, `.clang-format` |
| Change CI | [BUILDING](BUILDING.md), [RELEASING](RELEASING.md) | `.github/workflows/build.yml`, `pr-checks.yml` |
| Cut a release / sign macOS builds | [RELEASING](RELEASING.md) | `release-please-config.json`, `.github/workflows/release-please.yml`, `release.yml`, `install.iss` |
| Update user-facing download links | [DISTRIBUTION](DISTRIBUTION.md) | `Source/PropConnectScreen.cpp`, `Source/FirmwareChooserScreen.cpp` (help-article URLs) |

---

## One-paragraph orientation

`MainComponent` drives a linear wizard (choose prop type, connect, choose firmware, upload, done) over
three singletons. `PropManager` polls `hid_enumerate(0xF107, …)` at 10 Hz on the message thread and wraps
each bootloader-mode prop in a `Prop` thread that runs the lock-step HID flash protocol. `FirmwareManager`
mirrors the server's `.fwimg` catalog into a local cache, and `AppUpdater` checks `update.json` at startup.
Pure logic (version compare, `.fwimg` parsing, hardware-revision rules) lives in header-only
`VersionUtils.h` / `FirmwareImage.h` with Catch2 tests. The HID path cannot be tested without physical
props, so any change there ends with a hardware hand-off: flash a real Capsule and a Vision, and confirm
they boot the new firmware.
