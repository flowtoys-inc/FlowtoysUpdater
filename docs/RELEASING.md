# Releasing FlowtoysUpdater

A release is: a version bump, a git tag, CI-built artifacts on GitHub Releases, and (for existing users to auto-update) a manual publish to flow-toys.com. [release-please](https://github.com/googleapis/release-please) does the first three from Conventional Commit PR titles. This document is the full checklist.

## The artifact naming contract

The in-app self-updater (`Source/AppUpdater.cpp`) constructs download filenames as:

```
FlowtoysUpdater-<os>-<version>.<extension>
      os        = osx | win-x64 | linux
      extension = whatever update.json declares (zip by default)
```

Every part of the pipeline — the CMake project version, the git tag, the CI artifact names, and the server's `update.json` — must agree on `<version>`, or shipped apps will look for a file that doesn't exist. CI enforces tag == CMake version (the tag is `v<version>`); the rest of this checklist covers what CI can't reach.

## Release checklist

1. **Land changes on `master`** with Conventional Commit PR titles (`feat: …`, `fix: …`). PRs are squash-merged, so the title is the commit release-please reads. `feat` bumps the minor version, `fix` the patch, and `!` / `BREAKING CHANGE:` the major.
2. **release-please** (`.github/workflows/release-please.yml`) keeps a `chore(master): release x.y.z` PR open. It bumps the version in `CMakeLists.txt` (the line marked `# x-release-please-version`) and updates `CHANGELOG.md`. Never edit those, or `.release-please-manifest.json`, by hand. Release PRs get no CI, because release-please opens them with `GITHUB_TOKEN`.
3. **Dry run** the release from the release PR's branch by pushing it to the `release-dry-run` branch: `git push -f origin <branch-or-sha>:release-dry-run`. (Actions → Release → Run workflow does the same, but GitHub only offers it once `release.yml` is on the default branch, `master`.) It builds, signs and notarizes exactly like a real release (named with the CMake version) but only keeps the files as workflow artifacts — no tag check, no GitHub Release. Download `release-macos` and run the [verification checks](#verifying-a-signed-build) on it. Do this after any change to signing secrets or the workflow.
4. **Merge the release PR.** release-please tags `vx.y.z` and creates the GitHub Release with the changelog. It then calls `.github/workflows/release.yml` directly, because a tag pushed with `GITHUB_TOKEN` triggers no workflows. Tags pushed by hand trigger nothing. That workflow:
   - verifies the tag matches the CMake version (hard failure if not),
   - builds macOS (universal arm64+x86_64), Windows (x64, static CRT), Linux,
   - packages `FlowtoysUpdater-osx-x.y.z.zip`, `FlowtoysUpdater-osx-x.y.z.pkg` (pkgbuild, installs to `/Applications`, auto-launches after install), `FlowtoysUpdater-win-x64-x.y.z.exe` (Inno Setup), `FlowtoysUpdater-linux-x.y.z.zip`,
   - signs/notarizes the macOS artifacts **only if signing secrets are configured** (below),
   - attaches all artifacts to the GitHub Release.
5. **Smoke-test on hardware** — flash a real prop with the built app. CI cannot test the HID path.
6. **Publish to flow-toys.com** (manual until server automation exists):
   - upload the artifacts to `http://flow-toys.com/fusion/app/`,
   - update `http://flow-toys.com/fusion/update.json` (schema below). Existing installs poll this file — nothing reaches users until it changes.
7. **Update the user-facing download links** if they name a version: the Freshdesk help articles listed in [DISTRIBUTION.md](DISTRIBUTION.md). Edit them in place, because the app links to them by URL.

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
| `MACOS_CERTIFICATE` | base64 of a `.p12` containing the **Developer ID Application** certificate and, ideally, the **Developer ID Installer** certificate: `base64 -i certs.p12` |
| `MACOS_CERTIFICATE_PASSWORD` | password of that `.p12` |
| `NOTARY_APPLE_ID` | Apple ID email used for notarization (a team member's own Apple ID) |
| `NOTARY_PASSWORD` | app-specific password for that Apple ID (account.apple.com → Sign-In and Security → App-Specific Passwords) |
| `NOTARY_TEAM_ID` | the 10-character Team ID: `YLHB8LP34S` (Flowtoys, Inc.) |

What each combination produces:

- **Application cert + notary secrets:** signed (hardened runtime, timestamped) and notarized app, stapled in the zip and inside the pkg.
- **Installer cert too:** the `.pkg` is also signed, notarized and stapled. Without it the job warns and ships an unsigned `.pkg` around the signed app; the zip is unaffected.
- **No signing secrets:** ad-hoc signed app (right-click → Open on first launch), unsigned pkg, no notarization.

The job fails, rather than shipping something half-signed, if the `.p12` lacks a valid Developer ID Application identity or Apple rejects a notarization (Apple's log is printed).

### Getting the certificates (one-time)

Flowtoys, Inc. is enrolled as an organization (Team ID `YLHB8LP34S`). Only the team's **Account Holder** can create Developer ID certificates; Admins and other roles see them greyed out. Cloud-managed Developer ID certificates (Xcode, Admin role) can't be exported, so they don't work for CI.

So a maintainer who isn't the Account Holder does this, keeping the private key on their own Mac:

1. Keychain Access → Certificate Assistant → **Request a Certificate From a Certificate Authority**. Your email, a descriptive Common Name (e.g. `FlowtoysUpdater signing`), CA Email left blank, **Saved to disk**. The private key stays in your login keychain.
2. Send the `.certSigningRequest` to the Account Holder (it contains no secrets). At developer.apple.com → Certificates → **+**, they choose **Developer ID Application** (profile type **G2 Sub-CA**), upload the CSR, and send back the downloaded `.cer`.
3. **Repeat with a new CSR** (e.g. `FlowtoysUpdater installer`) for **Developer ID Installer**. Apple refuses to reuse a CSR for a second certificate.
4. Double-click each `.cer`. Check with `security find-identity -v -p basic`: both `Developer ID Application: Flowtoys, Inc. (YLHB8LP34S)` and `Developer ID Installer: …` should be listed.
5. Keychain Access → My Certificates → select both → Export 2 items → `.p12` with a strong password. Keep the `.p12` and password in a password manager; the private keys exist nowhere else.
6. Set the secrets (repo **write** access is enough), then delete the `.p12` from disk:
   ```bash
   base64 -i certs.p12 | gh secret set MACOS_CERTIFICATE -R flowtoys-inc/FlowtoysUpdater
   for s in MACOS_CERTIFICATE_PASSWORD NOTARY_APPLE_ID NOTARY_PASSWORD NOTARY_TEAM_ID; do gh secret set "$s" -R flowtoys-inc/FlowtoysUpdater; done
   ```

Before trusting the notary secrets in CI, check them locally: `xcrun notarytool store-credentials flowtoys --apple-id <you> --team-id YLHB8LP34S`, then `xcrun notarytool history --keychain-profile flowtoys` should succeed.

The Developer ID Application certificate issued in Oct 2026 expires **Nov 26, 2027**. Renew it before then with the same CSR steps. Builds already shipped keep working after expiry because their signatures are timestamped; only new signing stops.

### Verifying a signed build

On the downloaded artifact (dry run or release), on a Mac that didn't build it:

```bash
ditto -x -k FlowtoysUpdater-osx-x.y.z.zip /tmp/ftu
spctl --assess --type execute -vv /tmp/ftu/FlowtoysUpdater.app   # accepted, source=Notarized Developer ID
xcrun stapler validate /tmp/ftu/FlowtoysUpdater.app
spctl --assess --type install -vv FlowtoysUpdater-osx-x.y.z.pkg  # only if the Installer cert was present
```

Plus the usual launch checks from the bad-dylib incident (#24): `otool -L` on the binary shows only system libraries, and the quarantined download opens with a double-click.

Windows builds are not code signed, and won't be (decided Oct 2026): users click through SmartScreen's *More info → Run anyway* on first install.

1.2.0 was the last release cut by hand (tag `1.2.0`, without the `v`). release-please counts from its commit (`last-release-sha` in `release-please-config.json`), and `modernize` was merged into `master` with a merge commit so that commit stays in `master`'s history. Remove `last-release-sha` once release-please has made its first release. Repo setting needed once: Settings → Actions → General → **Allow GitHub Actions to create and approve pull requests**.

## Dependency updates

Dependabot (`.github/dependabot.yml`) opens one grouped PR per ecosystem a week against `master`, for GitHub Actions (SHA pins) and the `ThirdParty/` submodules (JUCE, hidapi). The Catch2 pin in `Tests/CMakeLists.txt` and the clang-format pin in `build.yml` are manual — check [Catch2 releases](https://github.com/catchorg/Catch2/releases) occasionally.
