# USB HID bootloader protocol

How the app flashes a prop. Everything here is implemented in `Source/Prop.{h,cpp}` and `Source/PropManager.cpp`; nothing in this protocol may change without coordinating with the firmware/bootloader on the props (backward compatibility is a hard requirement).

## Device identification

| | |
|---|---|
| USB Vendor ID | `0xF107` (all flowtoys props) |
| Product ID `0x1000` | Capsule 2.0 |
| Product ID `0x1001` | Vision / Club |

Every prop exposes two USB personalities: **app mode** (normal operation) and **bootloader mode** — distinguished by the USB *product string*, which contains `"bootloader"` in bootloader mode. The updater only ever talks to bootloaders:

- A **Vision/Club** found in app mode is sent `AppReset(Bootloader)` to reboot it into the bootloader automatically.
- A **Capsule** must be put into bootloader mode by hand (button sequence; the app links to instructions).

Discovery is a 10 Hz `hid_enumerate` poll; devices are tracked by USB serial number.

## Framing

All transfers are 64-byte HID output/input reports (`PACKET_SIZE = 64`). On write, a `0x00` report-ID byte is prepended (65 bytes handed to `hid_write`); unused bytes are zero-padded. All integers are **little-endian**.

## Commands (host → device)

| Command | Code | Payload after the u32 command word |
|---|---|---|
| `GetStatus` | `0x00` | — |
| `Update` | `0x01` | `u32 totalBytesToSend`, `u32 crc` (currently always 0), `u16 vid`, `u16 pid`, `u16 hw_rev` |
| `Reset` | `0x02` | — |
| `GetVersion` | `0x10` | `u32 subject` (1 = Bootloader, 2 = App) |
| `Data` | `0x80` | up to 60 bytes of firmware (`DATA_PACKET_MAX_LENGTH = PACKET_SIZE − 4`) |
| `AppReset` | `0xFF` | `u32 subject` — app→bootloader when sent to the app; bootloader→app to boot the new firmware after flashing |

## Responses (device → host)

Responses are read with a blocking `hid_read` of 64 bytes, strictly lock-step (one response per request, no correlation IDs).

**`GetStatus` response**: `u8 cmd`, 3 pad bytes, `u32 ackStatus`, `u8 status`, C-string message.
`ackStatus` doubles as the **byte offset the device has acknowledged** — it drives both erase progress and the write cursor during programming.

Status values: `NotSet 0`, `Idle 2`, `EraseBusy 3`, `ProgramIdle 4`, `ProgramBusy 5`, `ProgramDone 6`, `Error 10`.

**`GetVersion` response**: `u8 cmd`, `u8 active`, 2 pad, `u16 vid`, `u16 pid`, `u16 hw_rev`, `u16 fw_rev`, `u32 fw_date`, `char git_rev[8]`, `char fw_ident[20]`. The bootloader subject supplies vid/pid/hw_rev/git_rev; the app subject supplies fw_rev/fw_date/fw_ident. `fw_rev` encodes the version as `(major << 8) | minor`.

## Hardware revisions

`hw_rev` values map to revision letters: `0x300`=C, `0x400`=D, `0x500`=E, `0x600`=F, `0x700`=G, `0x800`=H. Firmware/hardware compatibility is an exact match, with one exception: **Capsule revisions C and D accept each other's firmware**. The connect screen refuses to proceed if connected props report differing hardware revisions.

## Flash sequence (per prop, one thread each)

1. `GetStatus`; if not `Idle`, send `Reset` and poll every 10 ms until `Idle`.
2. `Update(totalBytesToSend, crc=0, vid, pid, hw_rev)`.
3. **Erase phase**: poll `GetStatus` — `EraseBusy` → keep waiting; `Error` → abort; `ProgramIdle` → continue. Progress is estimated as `ackStatus / sizeToErase` with hardcoded erase sizes: **51200** bytes (Capsule), **113664** bytes (Vision/Club).
4. **Program phase**: while status is `ProgramIdle` and `ackStatus < totalBytesToSend`, send a `Data` packet containing the 60 bytes at offset `ackStatus` — i.e. the device's acknowledged offset *is* the write cursor, giving implicit retry/resume within a session. `ProgramDone` → finished.
5. `AppReset(App)` to boot the new firmware.

The firmware image is padded to a multiple of 60 bytes so every `Data` packet is full. There is currently no CRC and no post-flash verification (the `crc` field exists in the protocol but the app sends 0) — see issue #12.

## Failure behavior

- 20 consecutive empty reads → the prop is marked errored and the UI asks the user to unplug/replug and retry.
- A prop that disappears mid-flash simply stops responding (enumeration is paused during flashing); it stays in bootloader mode and can be re-flashed — the bootloader itself is not overwritten.
- Known defect: on macOS the blocking `hid_read` has no timeout, so a silently-dead device can hang its flash thread (issue #10).
