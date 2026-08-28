# FlowtoysUpdater — notes for Claude sessions

Desktop firmware updater for flowtoys LED props (JUCE 8, CMake, USB HID). GPLv3.

## Build

```bash
git submodule update --init                # JUCE + hidapi under ThirdParty/, pinned
cmake -B build -DCMAKE_BUILD_TYPE=Debug    # add -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" for universal
cmake --build build --parallel
ctest --test-dir build --output-on-failure # Catch2 unit tests
```

If only Command Line Tools are selected on the machine, prefix with
`DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer` (adjust to the installed Xcode; sudo-free).

## Hard constraints (backward compatibility)

- **Never change**: the HID protocol (`docs/HID-PROTOCOL.md`), the `.fwimg` format, USB VID/PIDs, the artifact naming contract `FlowtoysUpdater-{osx|win-x64|linux}-<version>.<ext>`, product name `FlowtoysUpdater`, bundle id `com.benkuper.flowtoysupdater`, or the firmware cache location. Fielded 1.x installs depend on all of these.
- The app's endpoints are plain HTTP on flow-toys.com; `NSAllowsArbitraryLoads` must stay until the server-side HTTPS migration (issue #12).
- The version number lives ONLY in `CMakeLists.txt` `project(VERSION)`; release tags must equal it (CI enforces).

## Workflow

- Integration branch is `modernize`; all work lands there via PRs from feature branches. `master` changes only by approved PR from `modernize`.
- GitHub Actions are pinned to commit SHAs (version in a trailing comment); dependabot maintains pins + submodules.
- Do not compile `Source/hid.c` / `Source/hid_osx.c` (legacy vendored hidapi; the submodule replaces them — deletion tracked in #7).

## Testing limits

Anything touching `hid_*` or the flash state machine cannot be verified without physical props (Capsule 2.0 / Vision). Say so explicitly rather than claiming verification. UI can be walked without hardware via Cmd+N (Debug builds). The firmware download path can be verified live — the production server is up and serves the full catalog.
