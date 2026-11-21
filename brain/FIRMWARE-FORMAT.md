# The .fwimg firmware format and server index

## .fwimg files

A `.fwimg` is a **ZIP archive** with exactly two entries (parsing: `Source/FirmwareImage.h`):

| Entry | Contents |
|---|---|
| `data` | the raw firmware image, streamed to the prop during flashing (padded to 60-byte packets on the wire) |
| `meta` | a JSON object describing the image |

### meta schema

```jsonc
{
  "usb_vid": 61703,        // 0xF107
  "usb_pid": 4096,         // 0x1000 Capsule, 0x1001 Vision/Club — selects the prop type
  "fw_rev": 522,           // version as (major << 8) | minor; 522 = 2.10
  "hw_rev": 1280,          // target hardware revision; 0x300=C … 0x800=H
  "fw_date": 1580000000,   // build date, unix seconds
  "git_rev": "abc1234",    // firmware source revision
  "fw_ident": "capsule"    // human-readable identity
}
```

A file that is not a zip, lacks either entry, or whose `meta` is not a JSON **object** is rejected and deleted, including a local file the user picked via "Choose local file" (`FirmwareManager::getFirmwareForFile`). Compatibility against the connected prop is an exact `hw_rev` match, except Capsule C↔D which are interchangeable.

## Server index

From the release after 1.2.1, the app mirrors firmware from the web updater's catalog, over HTTPS
(`Source/FirmwareCatalog.h`, `FirmwareManager::run`):

- **Index**: `GET https://updater.flowtoys.com/firmware/index.json` (S3 + CloudFront, 60 s cache):
  ```jsonc
  { "schema": 1, "generated": "…", "firmware": [
    { "file": "flowOS 2.10-spin17-e.fwimg", "size": 66230, "sha256": "<lowercase hex>",
      "vid": 61703, "pid": 4097, "hw_rev": 1280, "fw_rev": 522, "fw_ident": "flowOS/spin17", "…": "…" } ] }
  ```
  The app reads only `file`, `size` and `sha256` and ignores other keys. Anything but `schema: 1`, a
  file name that isn't a plain `*.fwimg` name in one path segment, or a missing size or sha256
  rejects the whole index; the app then uses its local cache.
- **Files**: `GET https://updater.flowtoys.com/firmware/<encodeURIComponent(file)>`. Names are
  immutable, so a cached file with the index's size and sha256 is current; any other cached or
  downloaded file is deleted and fetched again.
- There is no `clear` flag any more.

**1.2.1 and older** read `http://flow-toys.com/fusion/firmwares.php`
(`{"files": [...], "clear": false}`) and `/fusion/firmwares/<name>` over plain HTTP. Those URLs are
compiled into fielded installs, so `flow-toys.com/fusion/*` must keep serving the same catalog for as
long as old installs matter, wherever it is hosted.

Files are cached in `userApplicationDataDirectory/FlowtoysFirmwares` (macOS: `~/Library/FlowtoysFirmwares`), revalidated on launch and hourly. Users can also flash a local `.fwimg` via "Choose local file" — the same parser and hardware checks apply, with an override prompt for hardware-revision mismatches.

## Conventions observed in production filenames

`flowOS <version>-<ident>-<hwrev letter>.fwimg`, e.g. `flowOS 2.10-club (short handle, delphin)-e.fwimg`. The name is display metadata only; everything the app trusts comes from `meta`.
