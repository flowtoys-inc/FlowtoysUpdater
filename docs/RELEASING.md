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
   - signs/notarizes the macOS artifacts and Authenticode-signs the Windows exe and installer, each **only if its signing secrets are configured** (below),
   - attaches all artifacts to the GitHub Release.
5. **Smoke-test on hardware** — flash a real prop with the built app. CI cannot test the HID path.
6. **Publish to flow-toys.com** (manual until server automation exists):
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

## Windows signing (Azure Artifact Signing)

Unsigned Windows builds make SmartScreen say "Windows protected your PC" with **Publisher: Unknown publisher**, and the installer's UAC prompt shows the same. The release workflow signs both `FlowtoysUpdater.exe` (before Inno Setup packs it) and the `FlowtoysUpdater-win-x64-x.y.z.exe` installer with Authenticode, RFC 3161 timestamped, then checks both with `signtool verify /pa` and fails if either lacks a timestamp. Like macOS, it's activated purely by secrets; without them the job warns and ships unsigned files. Tracked in issue #4.

### Why Artifact Signing

Since June 2023, code-signing private keys must live on certified hardware (a USB token or a cloud HSM), so a `.pfx` in a GitHub secret is no longer possible. The options that work in CI (prices checked Oct 2026):

| Option | Approx. cost | CI fit | Notes |
|---|---|---|---|
| **Azure Artifact Signing** (formerly Trusted Signing), Basic | **$9.99/month** (5,000 signatures/month) | Official GitHub Action, OIDC, no long-lived secret | Microsoft is the CA. Organizations in the US and several other countries are eligible. Certificates last 3 days and rotate automatically, so there's nothing to renew. Validation takes 1–20 business days. |
| OV cert + SSL.com eSigner | $129/yr cert + eSigner from $15–20/month ≈ $310–370/yr | CodeSignTool or the eSigner action, with a TOTP secret in CI | Has to be renewed. The yearly signing quota is low. |
| OV/EV cert + DigiCert KeyLocker | ≈ $500–700+/yr | KeyLocker's `smctl` tooling | The most expensive. Has to be renewed. |
| EV certificate (any CA) | $300–400+/yr more than OV | as above | Since 2024 EV **no longer bypasses SmartScreen**. It earns reputation the same way OV does, so it isn't worth the premium. |
| SignPath Foundation (free for OSS) | free | SignPath's own pipeline | The certificate names **SignPath Foundation**, not Flowtoys, Inc., so it's wrong for a company product. |

So we use Artifact Signing: it's the cheapest, needs no token or renewal, and authenticates from CI by OIDC, so no secret in the repo could leak a signing key. Signatures read **Flowtoys, Inc.**, Emeryville, CA, US.

**SmartScreen reputation still builds over time.** Neither Artifact Signing nor any OV/EV certificate makes the warning go away on day one. Signing changes "Unknown publisher" to "Flowtoys, Inc." right away. The warning itself fades as signed downloads accumulate reputation for that publisher identity, which then carries over to later releases signed by the same identity. To speed this up, submit each new installer as a *Software developer* at <https://www.microsoft.com/wdsi/filesubmission>. Updates the app downloads itself are probably less affected, because the in-app download doesn't add the browser's mark-of-the-web; the UAC prompt will show the verified publisher either way.

**Known gap:** the Inno Setup *uninstaller* (`unins000.exe`) stays unsigned. Signing it needs Inno's `SignTool` directive with signtool + the Artifact Signing dlib, which we haven't wired up. It only shows up as the uninstall UAC prompt.

### What Flowtoys, Inc. has to do (one-time)

The person doing identity validation represents the company, so this should be a Flowtoys officer (e.g. Prisna). The technical steps can be handed to a maintainer.

1. **Azure account and paid subscription.** At <https://portal.azure.com>, create a pay-as-you-go subscription with a company card. Free, trial or sponsored subscriptions don't work. The cost is $9.99/month for the Basic SKU, plus negligible signing overage (≈ 2 signatures per release).
2. **Register the provider and create the account.** Subscriptions → Resource providers → register `Microsoft.CodeSigning`. Then create an **Artifact Signing account** with SKU Basic, in a US region such as East US (endpoint `https://eus.codesigning.azure.net`).
3. **Organization identity validation** (portal only). Assign yourself the **Artifact Signing Identity Verifier** role on the account. Then go to Identity validations → Organization → New identity → **Public**, and enter:
   - Organization name: the exact legal name, **Flowtoys, Inc.**, matching the state registration
   - Website: flowtoys.com
   - **Primary and secondary email on the company's own domain**, monitored, accepting external links. The verification links expire in 7 days.
   - Business identifier: the EIN or D-U-N-S number
   - Registered business address: Emeryville, CA
   - First and last name of the representative, exactly as on their government ID

   When the status changes to *Action Required*, the representative verifies their identity through AU10TIX (photo of their ID, on a phone) and Microsoft Authenticator. Microsoft may then ask for documents issued within the last 12 months: articles of incorporation or a state registration, and a domain registration or invoice naming the company. Up-to-date public records (California Secretary of State, D-U-N-S) make this faster.
4. **Certificate profile.** Once validation is *Completed*: Certificate profiles → Create → **Public Trust**, named e.g. `flowtoys-public`, using that identity validation.
5. **CI identity** (a maintainer can do this with Owner on the resource group and the *Application Developer* Entra role):
   - Microsoft Entra ID → App registrations → New registration `FlowtoysUpdater-release-signing`.
   - Certificates & secrets → Federated credentials → GitHub Actions: organization `flowtoys-inc`, repository `FlowtoysUpdater`, entity **Environment**, name **`release`**. The subject is then `repo:flowtoys-inc/FlowtoysUpdater:environment:release`. No client secret is created.
   - On the certificate profile (or account), assign that app the **Artifact Signing Certificate Profile Signer** role.
6. **Repository secrets** (repo write access is enough). None of them are signing keys:

   | Secret | Value |
   |---|---|
   | `AZURE_CLIENT_ID` | the app registration's Application (client) ID |
   | `AZURE_TENANT_ID` | the Directory (tenant) ID |
   | `ARTIFACT_SIGNING_ENDPOINT` | the account's region endpoint, e.g. `https://eus.codesigning.azure.net` |
   | `ARTIFACT_SIGNING_ACCOUNT` | the Artifact Signing account name |
   | `ARTIFACT_SIGNING_PROFILE` | the certificate profile name |

   ```bash
   for s in AZURE_CLIENT_ID AZURE_TENANT_ID ARTIFACT_SIGNING_ENDPOINT ARTIFACT_SIGNING_ACCOUNT ARTIFACT_SIGNING_PROFILE; do gh secret set "$s" -R flowtoys-inc/FlowtoysUpdater; done
   ```

7. **Optional hardening.** GitHub creates the `release` environment the first time the workflow runs. Under Settings → Environments → `release` → Deployment branches and tags, limit it to the `release-dry-run` branch and version tags, so no other branch can obtain a signing token.

Then do a dry run (`git push -f origin <ref>:release-dry-run`) and check the `Verify signatures` step.

### Verifying a signed Windows build

On a Windows machine, on the downloaded `release-windows` artifact:

```powershell
signtool verify /pa /v FlowtoysUpdater-win-x64-x.y.z.exe      # "Successfully verified", timestamped
Get-AuthenticodeSignature FlowtoysUpdater-win-x64-x.y.z.exe | Format-List Status,SignerCertificate,TimeStamperCertificate
```

Or right-click → Properties → Digital Signatures should list **Flowtoys, Inc.** with a timestamp. After installing, check `C:\Program Files\FlowtoysUpdater\FlowtoysUpdater.exe` the same way.

1.2.0 was the last release cut by hand (tag `1.2.0`, without the `v`). release-please counts from its commit (`last-release-sha` in `release-please-config.json`), and `modernize` was merged into `master` with a merge commit so that commit stays in `master`'s history. Remove `last-release-sha` once release-please has made its first release. Repo setting needed once: Settings → Actions → General → **Allow GitHub Actions to create and approve pull requests**.

## Dependency updates

Dependabot (`.github/dependabot.yml`) opens one grouped PR per ecosystem a week against `master`, for GitHub Actions (SHA pins) and the `ThirdParty/` submodules (JUCE, hidapi). The Catch2 pin in `Tests/CMakeLists.txt` and the clang-format pin in `build.yml` are manual — check [Catch2 releases](https://github.com/catchorg/Catch2/releases) occasionally.
