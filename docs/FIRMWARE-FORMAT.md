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

A file that is not a zip, lacks either entry, or whose `meta` is not a JSON **object** is rejected (and, in the manager's cache path, deleted). Compatibility against the connected prop is an exact `hw_rev` match, except Capsule C↔D which are interchangeable.

## Server index

The app mirrors firmware from `http://flow-toys.com/fusion/`:

- **Index**: `GET /fusion/firmwares.php` returns
  ```jsonc
  { "clear": false, "files": ["flowOS 2.10-spin17-e.fwimg", "..."] }
  ```
  `clear: true` makes clients delete their entire local cache before re-downloading (use with care — this is an unauthenticated HTTP endpoint; see issue #12).
- **Files**: each name in `files` is fetched from `/fusion/firmwares/<url-escaped name>`.

Files are cached in `userApplicationDataDirectory/FlowtoysFirmwares` (macOS: `~/Library/FlowtoysFirmwares`), revalidated on launch and hourly. Files already present and parseable are not re-downloaded. Users can also flash a local `.fwimg` via "Choose local file" — the same parser and hardware checks apply, with an override prompt for hardware-revision mismatches.

## Conventions observed in production filenames

`flowOS <version>-<ident>-<hwrev letter>.fwimg`, e.g. `flowOS 2.10-club (short handle, delphin)-e.fwimg`. The name is display metadata only; everything the app trusts comes from `meta`.
