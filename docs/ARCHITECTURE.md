# Architecture

FlowtoysUpdater is a small JUCE GUI app (~1k lines of application code). It is a linear five-step wizard driven by three global singletons.

## The wizard

`MainComponent` owns a `ScreenManager`, which owns the five screens and advances linearly (`Screen.h` / `ScreenManager.cpp`):

| # | Screen | What happens |
|---|---|---|
| 1 | `PropChooserScreen` | Pick prop type: Capsule 2.0 (PID `0x1000`) or Vision/Club (PID `0x1001`) |
| 2 | `PropConnectScreen` | Wait for props on USB; shows per-type reset instructions. "Next" needs ≥1 prop, all with the same hardware revision |
| 3 | `FirmwareChooserScreen` | Pick a downloaded firmware (filtered by hardware compatibility) or a local `.fwimg` file |
| 4 | `UploadScreen` | Flash all connected props in parallel; progress bar |
| 5 | `EndScreen` | Done; restart or flash more props of the same type |

Each screen's real work is triggered from its `reset()` (called on entry). In Debug builds, Cmd+N skips to the next screen for UI testing without hardware.

## The singletons

- **`PropManager`** — device discovery and flashing. Polls `hid_enumerate(0xF107, ...)` at 10 Hz on a message-thread timer. Only talks to props whose USB product string contains "bootloader"; a Vision/Club found in app mode is automatically sent an `AppReset` command to reboot into its bootloader (Capsules must be reset by hand — the connect screen shows how). Each connected prop is a `Prop`, which is a `juce::Thread` running the flash state machine — so a hub of props flashes in parallel. See [HID-PROTOCOL.md](HID-PROTOCOL.md).
- **`FirmwareManager`** — firmware acquisition and parsing. A background thread fetches the index at `http://flow-toys.com/fusion/firmwares.php` and mirrors all `.fwimg` files into `userApplicationDataDirectory/FlowtoysFirmwares`, re-checking hourly. Parsing lives in `Source/FirmwareImage.h`. See [FIRMWARE-FORMAT.md](FIRMWARE-FORMAT.md).
- **`AppUpdater`** — self-update. Checks `http://flow-toys.com/fusion/update.json` at startup; on a newer version, offers to download and replace/reinstall the app. See [RELEASING.md](RELEASING.md) for the contract.

Pure logic shared by these (version comparison, download filenames, `.fwimg` parsing, hardware-revision rules) is extracted into `Source/VersionUtils.h` and `Source/FirmwareImage.h`, which depend only on `juce_core` and are covered by the unit tests in `Tests/`.

## Threads and events

Threads: the JUCE message thread (UI + the 10 Hz device poll), one `FirmwareManager` thread, one `AppUpdater` thread, one thread per `Prop` while flashing, plus JUCE `DownloadTask` and hidapi-internal threads.

Cross-thread UI notification goes through `QueuedNotifier<T>` (`Source/QueuedNotifier.h`), a FIFO + `AsyncUpdater` event bus: background threads post event objects, listeners receive them on the message thread. Screens subscribe to the managers' notifiers and repaint on events.

### Known threading debts (tracked in issues)

- `FirmwareManager::selectedFirmware` is a raw pointer into an `OwnedArray` that the hourly refresh clears — use-after-free risk, #9.
- `Prop::readResponse` uses a blocking `hid_read` with no timeout; a silent device hangs its flash thread, #10.
- The 10 Hz `hid_enumerate` runs on the message thread; `resetPropToBootloader` leaks a HID handle per call, #10.

## Layout

```
CMakeLists.txt          build definition; THE version number lives here
Source/                 app code (screens, managers, Prop protocol, extracted logic headers)
Tests/                  Catch2 unit tests
ThirdParty/JUCE         JUCE framework (pinned submodule)
ThirdParty/hidapi       hidapi (pinned submodule)
Resources/              icons and prop images (compiled in via juce_add_binary_data)
.github/workflows/      CI (build.yml) and release (release.yml)
install.iss             Windows installer (Inno Setup, built in CI)
docs/                   you are here
```

Historical note: the repo previously used JUCE 5.4.7 via Projucer-generated projects (`Builds/`, `JuceLibraryCode/`, `FlowtoysUpdater.jucer`) with Travis CI, and vendored a 2013 hidapi (`Source/hid.c`, `Source/hid_osx.c`, `External/`). Those were replaced in the 2026 modernization ([PLAN.md](PLAN.md)); leftovers are removed per issue #7.
