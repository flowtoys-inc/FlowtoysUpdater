# Distribution: how users get the updater

Where the download links live, what users see today, and how to keep them current. Observed Oct 28 2026 unless noted.

## Where users find it today

Users reach the updater through this chain, and every link in it is maintained by hand:

1. **flowtoys.com → Product Support** (Shopify, `https://flowtoys.com/pages/product-support`). Its *UPDATE CAPSULES* and *UPDATE VISION* buttons go to Freshdesk.
2. **Freshdesk articles** (`flowtoys2.freshdesk.com`). These are the actual user instructions:

   | Article | ID | What it ships |
   |---|---|---|
   | How to update your Capsule 2.0 firmware | `6000213534` | attachment `FlowtoysUpdater-osx-1.1.9.pkg.zip` |
   | How to update your Vision prop firmware | `6000213554` | attachment `FlowtoysUpdater-osx-1.1.9.pkg.zip` (text still says "1.1.3") |
   | Firmware updater – Windows | `6000228631` | attachment `FlowtoysUpdater-win-x64-1.1.9.exe.zip` |
   | Firmware updater troubleshooting | `6000216768` | text only; tells Mac users to right-click → Open |

   The app itself links to `6000213534`, `6000213554`, `6000216768` and two reset articles (`Source/PropConnectScreen.cpp`, `Source/FirmwareChooserScreen.cpp`). Those URLs are compiled into every fielded app, so **edit these articles in place. Never delete them or replace them with new ones.** Freshdesk resolves an article by its numeric ID, so renaming the title doesn't break links.
3. **flow-toys.com** (GoDaddy cPanel). It serves only the app's machine endpoints: `/fusion/update.json`, `/fusion/app/` (installers for the self-updater) and the firmware files. The site root returns 403 and HTTPS has a self-signed certificate, so it isn't a page users can be sent to.
4. **GitHub Releases** (`https://github.com/flowtoys-inc/FlowtoysUpdater/releases`). The repo is public. Every release has the signed and notarized Mac `.pkg`/`.zip`, the Windows `.exe` and the Linux `.zip`.

So a new user installs **1.1.9**, which is unsigned. On a Mac that means the Gatekeeper right-click dance, and on Windows a SmartScreen warning. The app then self-updates to 1.2.0 via `update.json`. On Mac that self-update has been confirmed; on Windows it is untested.

## Step 1: point the instructions at 1.2.0 (now, no code)

Edit the Freshdesk articles and keep their URLs:

- **Downloads.** Link to the GitHub release files over HTTPS, not to `http://flow-toys.com/fusion/app/`. Browsers warn about or block plain-HTTP downloads linked from HTTPS pages. Until Step 2 lands, the links are version-specific:
  - macOS (Apple Silicon and Intel): `https://github.com/flowtoys-inc/FlowtoysUpdater/releases/download/1.2.0/FlowtoysUpdater-osx-1.2.0.pkg`
  - Windows (64-bit): `https://github.com/flowtoys-inc/FlowtoysUpdater/releases/download/1.2.0/FlowtoysUpdater-win-x64-1.2.0.exe`

  Alternatively, replace the attachments with the 1.2.0 files. That works, but the attachments go stale on every release.
- **Troubleshooting article.** Drop the Mac "unidentified developer / right-click → Open" paragraph, because 1.2.0 is notarized. Replace it with the Windows SmartScreen note: *"Windows protected your PC" → More info → Run anyway*. It stays permanently (see below).
- **Vision article.** Remove "this version says 1.1.3".
- **Windows article.** It's an attachment-only stub; put the direct link in it.

Nothing on cPanel has to change for Step 1. `update.json` and `/fusion/app/` already serve 1.2.0 to existing installs.

## Step 2: links that never go stale (small CI change)

Have `release.yml` also attach version-free copies of the installers to each GitHub Release, for example `FlowtoysUpdater-macOS.pkg` and `FlowtoysUpdater-Windows.exe`. GitHub then serves the newest one at a permanent URL:

```
https://github.com/flowtoys-inc/FlowtoysUpdater/releases/latest/download/FlowtoysUpdater-macOS.pkg
https://github.com/flowtoys-inc/FlowtoysUpdater/releases/latest/download/FlowtoysUpdater-Windows.exe
```

The articles then never need touching on a release. These copies are additions: the versioned names the self-updater depends on are unchanged.

## Windows: no code signing

The Windows installer is not, and won't be, code signed (decided Oct 28 2026). SmartScreen will keep showing *Windows protected your PC* on first install, so the instructions need to say *More info → Run anyway* for as long as the desktop app ships.

## Longer term: web app

On Oct 28 2026 the team concluded that the updater should become a web app (prototype in #27). WebHID works in Chrome and Edge on desktop, but not in Safari or Firefox. Once it ships, it becomes the single link in the instructions for both operating systems.
