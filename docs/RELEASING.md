# Releasing FlowtoysUpdater

A release is: a version bump, a git tag, CI-built artifacts on GitHub Releases, and (for existing users to auto-update) a manual publish to flow-toys.com. This document is the full checklist.

## The artifact naming contract

The in-app self-updater (`Source/AppUpdater.cpp`) constructs download filenames as:

```
FlowtoysUpdater-<os>-<version>.<extension>
      os        = osx | win-x64 | linux
      extension = whatever update.json declares (zip by default)
```

Every part of the pipeline — the CMake project version, the git tag, the CI artifact names, and the server's `update.json` — must agree on `<version>`, or shipped apps will look for a file that doesn't exist. CI enforces tag == CMake version; the rest of this checklist covers what CI can't reach.

## Release checklist

1. **Bump the version** in `CMakeLists.txt`: `project(FlowtoysUpdater VERSION x.y.z)`. Land that on `modernize`/`master` via PR.
2. **Tag** the release commit: `git tag x.y.z && git push origin x.y.z`.
3. CI (`.github/workflows/release.yml`) then:
   - verifies the tag matches the CMake version (hard failure if not),
   - builds macOS (universal arm64+x86_64), Windows (x64, static CRT), Linux,
   - packages `FlowtoysUpdater-osx-x.y.z.zip`, `FlowtoysUpdater-osx-x.y.z.pkg` (pkgbuild, installs to `/Applications`, auto-launches after install), `FlowtoysUpdater-win-x64-x.y.z.exe` (Inno Setup), `FlowtoysUpdater-linux-x.y.z.zip`,
   - signs/notarizes the macOS artifacts **only if signing secrets are configured** (below),
   - creates a GitHub Release with all artifacts.
4. **Smoke-test on hardware** — flash a real prop with the built app. CI cannot test the HID path.
5. **Publish to flow-toys.com** (manual until server automation exists):
   - upload the artifacts to `http://flow-toys.com/fusion/app/`,
   - update `http://flow-toys.com/fusion/update.json` (schema below). Existing installs poll this file — nothing reaches users until it changes.

## update.json schema (what the client actually reads)

```jsonc
{
  "version": "1.2.0",            // latest version; compared against the running app
  "testing": false,               // true = release builds ignore this file (staging flag)
  "changelog": ["...", "..."],   // lines for the latest version
  "archives": [                   // older versions, newest last; shown as history
    { "version": "1.1.9", "changelog": ["..."] }
  ],
  // Per-OS: if <os>Installer is non-empty, the updater downloads
  // FlowtoysUpdater-<os>-<version>.<osInstaller> and runs it as an installer.
  // Otherwise it downloads ...<osExtension> (default "zip") and replaces itself.
  "osxInstaller": "pkg", "winInstaller": "exe", "linuxInstaller": "",
  "osxExtension": "zip", "winExtension": "zip", "linuxExtension": "zip"
}
```

Keys are read in `AppUpdater::run()`. All traffic is plain HTTP today (see issue #12 for the HTTPS/signing plan) — keep the HTTP endpoint alive for fielded clients even after any HTTPS migration.

## macOS signing & notarization

Unsigned builds work locally but are Gatekeeper-blocked when downloaded. The release workflow has signing built in, activated purely by adding repository secrets — no workflow changes needed:

| Secret | Contents |
|---|---|
| `MACOS_CERTIFICATE` | base64 of a `.p12` containing **Developer ID Application** (and Installer) certificates: `base64 -i certs.p12` |
| `MACOS_CERTIFICATE_PASSWORD` | password of that `.p12` |
| `NOTARY_APPLE_ID` | Apple ID email used for notarization |
| `NOTARY_PASSWORD` | app-specific password for that Apple ID (appleid.apple.com → App-Specific Passwords) |
| `NOTARY_TEAM_ID` | the 10-character Team ID |

### Getting the certificates (one-time)

1. Enroll in the [Apple Developer Program](https://developer.apple.com/programs/enroll/) — $99/year. Prefer enrolling **flowtoys as an organization** (requires a D-U-N-S number and someone with signing authority; certificates then say "flowtoys" rather than a person's name). Individual enrollment also works and is faster.
2. In Xcode (Settings → Accounts → Manage Certificates) or at developer.apple.com, create **Developer ID Application** and **Developer ID Installer** certificates.
3. Export both (with private keys) from Keychain Access as a single `.p12`, then fill in the secrets above.

Windows signing (against SmartScreen warnings) needs an OV/EV code-signing certificate and a `signtool` step in the workflow — tracked in issue #4.

## Dependency updates

Dependabot (`.github/dependabot.yml`) opens weekly PRs against `modernize` for GitHub Actions (SHA pins) and the `ThirdParty/` submodules (JUCE, hidapi). The Catch2 pin in `Tests/CMakeLists.txt` is manual — check [Catch2 releases](https://github.com/catchorg/Catch2/releases) occasionally.
