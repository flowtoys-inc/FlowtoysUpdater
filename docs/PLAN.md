# FlowtoysUpdater Modernization Plan

This document lays out the plan for bringing FlowtoysUpdater back to buildable, releasable health — starting with a native Apple Silicon build — after a deep review of the codebase in August 2026. Work is tracked in GitHub issues (#1–#12) and lands on the `modernize` integration branch via reviewed PRs; `master` is only updated from `modernize` by an approved PR at the end.

## Where the project stands

FlowtoysUpdater is a small JUCE desktop app (~820 lines of application code) that flashes firmware onto Flowtoys props (Capsule 2.0, Vision) over USB HID. The last release was 1.1.9 in August 2020. Since then the ecosystem it depended on has disappeared:

- **CI is gone.** Travis CI for open source shut down in 2021. The `.travis.yml` pipeline (macOS + Linux; Windows was always manual) can never run again.
- **The build system no longer works.** The Xcode project targets macOS 10.7 (modern Xcode's floor is 10.13) and pins the legacy build system that Xcode 14 removed. The Projucer 5.4.7 that generated the projects is effectively unobtainable.
- **JUCE 5.4.7 predates Apple Silicon** (support arrived in JUCE 6.0.2), and the JUCE used by CI was an unpinned clone of a personal fork (`benkuper/JUCE@develop-local`) expected at `~/JUCE` — not reproducible today.
- **The vendored HID library is from 2013** (the abandoned `signal11/hidapi`). On macOS it contains a known bug: it dlopens a wrong IOKit path, and the fallback reinterpret-casts an opaque `IOHIDDeviceRef` to a guessed struct layout — undefined behavior that happened to work on Intel Macs and is a genuine landmine on arm64.
- **Artifacts are unsigned and unnotarized**, which modern macOS Gatekeeper blocks outright for downloaded apps; macOS releases were also shipped as Debug builds.
- Housekeeping: `master` sat 3 commits behind the released 1.1.9 tag (the new `modernize` branch starts from 1.1.9, fixing this), the README has been 2 lines since 2018, and there are no tests of any kind.

The review also surfaced a handful of latent bugs — a startup crash on malformed cached firmware files (#8), a use-after-free when the hourly firmware refresh runs (#9), flash threads that can hang forever on a silent device (#10), firmware "1.10" sorting below "1.7" (#11) — and one structural security issue: updates and firmware travel over plain HTTP with no signatures (#12, needs server-side coordination).

## Guardrails: backward compatibility

Everything already in the field keeps working:

- **Self-update chain**: artifact names stay exactly `FlowtoysUpdater-{osx|win-x64|linux}-<version>.<ext>`; app name, bundle id (`com.benkuper.flowtoysupdater`), and `/Applications` install location unchanged. Existing installs poll `http://flow-toys.com/fusion/update.json` — that HTTP endpoint must stay alive (any HTTPS move runs in parallel, #12).
- **Props & protocol**: zero changes to the HID protocol — VID/PIDs, 64-byte report framing, bootloader command set, erase sizes, hardware-revision compatibility rules. Flash behavior stays byte-identical.
- **Firmware ecosystem**: the `.fwimg` format, the `firmwares.php` index contract, and the local firmware cache location are unchanged.
- **Old machines**: macOS builds are universal (arm64 + x86_64) targeting 10.13. Windows stays x64, now with a static C runtime (removes the redistributable requirement). The 1.1.9 artifacts remain available for anything newer toolchains can no longer target.
- **Behavior**: no UI/UX redesigns; code changes limited to build/API migration and fixes for crashes or memory errors.

## Phase 1 — Native Apple Silicon build (#1)

The core move is migrating from Projucer-generated projects to **JUCE's native CMake support**, which fixes several problems at once: scriptable CI builds, one place for the version number (previously duplicated across 9 files per release), no Projucer dependency, and first-class `arm64;x86_64` universal builds.

- Pin **JUCE 8** (newest stable tag) and **libusb/hidapi** (maintained successor of signal11/hidapi; drop-in compatible with every HID call the app makes) as git submodules under `ThirdParty/`.
- One root `CMakeLists.txt`: `juce_add_gui_app` reproducing today's app identity (name, bundle id, icon, the ATS plist key the HTTP endpoints require), `juce_add_binary_data` for the images (replacing a committed 7.4 MB generated file), per-platform hidapi linkage. `Source/hid.c` / `Source/hid_osx.c` and the prebuilt Windows/macOS HID libraries are no longer compiled.
- A small, fully enumerated set of JUCE 5→8 API fixes (two `URL::createInputStream` call sites, two `URL::downloadToFile` call sites, two `JuceHeader.h` include paths).
- One safety fix folded in (#8): null-guards in `.fwimg` parsing, extending the intent of the 1.1.7 "fix crash on bad files" commit.

**Verification**: universal binary check, native arm64 launch, full UI walkthrough without hardware (live firmware-index fetch proves the networking migration), then an end-to-end flash test on physical props — including watching for new macOS input-monitoring permission prompts, since HID access rules have tightened since 2020.

## Phase 2 — CI and releases on GitHub Actions (#2, #3, #7)

- `build.yml`: PR/push matrix (macOS / Windows / Linux), Release builds, tests, unsigned artifacts. Windows gets CI for the first time in the project's history.
- `release.yml` on tag push: a **version gate** (tag must equal the CMake project version — previously nothing checked this, and a mismatch silently publishes an artifact the self-updater can never find), packaging per platform (macOS `ditto` zip + `pkgbuild` .pkg replacing the Packages.app dependency; Windows Inno Setup with static CRT; Linux zip), published to GitHub Releases. Uploading to flow-toys.com and editing the server's `update.json` remain documented manual steps until server access is arranged.
- Signing/notarization (#4) is structurally present but gated on secrets, so it slots in as soon as an Apple Developer ID exists — ideally Flowtoys enrolling as an organization.
- Once CI is green on all platforms: a reviewable cleanup PR deletes the dead build system (`.travis.yml`, `Builds/`, `JuceLibraryCode/`, `External/`, the `.jucer`, Packages project) (#7).

## Phase 3 — Tests (#5)

Catch2 + CTest wired into CI. First targets are the pure logic that needs no hardware: version comparison, artifact naming, hardware-revision compatibility (including the Capsule C/D interchange), firmware sorting (encoding the #11 bug as a failing-then-fixed test), and `.fwimg` parsing against malformed-input fixtures (locking in #8). The flash state machine becomes testable later behind a small transport interface — that's the only way to exercise disconnect/error paths without unplugging hardware mid-flash.

## Phase 4 — Documentation (#6)

README rewrite plus `docs/`: BUILDING (submodules, per-OS deps), RELEASING (tag flow, artifact contract, the `update.json` schema the client actually reads, signing setup), ARCHITECTURE, HID-PROTOCOL (the full command set and flash state machine, currently undocumented), FIRMWARE-FORMAT (`.fwimg` spec).

## Phase 5 — Follow-up roadmap (tracked, not in this effort)

Ordered by risk × value: the firmware-list use-after-free (#9); flash-thread hangs and lifecycle (#10); HTTPS + signed updates/firmware, coordinated with the flow-toys.com server (#12); the HID handle leak and moving device polling off the UI thread (#10); version-handling cleanup (#11); AppImage revival.

## Notes on licensing

The app is GPLv3. JUCE 8 is AGPLv3 for open-source use, which a GPLv3 project may link against (GPLv3 §13); the README will note the combined-work terms. If Flowtoys prefers, relicensing the app itself to AGPLv3 is an owner decision — not assumed here.
