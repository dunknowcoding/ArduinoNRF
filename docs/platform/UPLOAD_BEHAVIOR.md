# Upload Behavior

Date: 2026-06-09

This document records the current upload truth exposed by the package. It does not claim that every clone board is fully verified.

## Native USB group

These boards currently declare USB-backed upload support in package metadata:

- `promicro_nrf52840`
- `nicenano_v2`
- `supermini_nrf52840`
- `nrfmicro_nrf52840`
- `mini_nrf52840`
- `xiao_nrf52840`
- `devboard_nrf52840`
- `pitaya_go_nrf52840`
- `usb_dongle_nrf52840`

## Current Windows wrapper behavior

- `tools/niusrobotlab/upload.ps1` owns the touch/reset sequence on Windows instead of relying on the Arduino CLI default touch path.
- `Bootloader / DFU → Auto-detect` prefers a matching UF2 mass-storage volume when the selected board is already in bootloader mode. Explicit serial-DFU menu entries still use `adafruit-nrfutil`.
- UF2 drives are matched to the selected serial port by stable USB identity. If two boards expose the same volume label, the wrapper refuses ambiguous matches instead of choosing the first drive.
- `Upload Method → Enter UF2 drive only (no upload)` performs the touch/bootloader wait, reports the matched drive, and exits before copying firmware.
- If the selected upload COM is stale after a mode change, the wrapper fails with a clear "re-select the current SERVICE/DFU port" message instead of falling through to another board.
- The wrapper treats same-PID runtime/bootloader cases as a separate class, rather than assuming a visible `0x239A:0x00B3` COM is already a bootloader port. On these boards it does NOT block the DFU on a PnP-level "transition observed" signal (because runtime and bootloader share `0x00B3`) — it surfaces a `[warn] ... Port never detached after touch` informational line and proceeds with a direct DFU attempt, which now succeeds because the firmware-side touch fix lands the chip in the bootloader before the warn fires.
- The previous service-port "boot token" fallback (`~NIUSBL!42\r` after arming with line coding `134/8/2/2 + DTR+RTS`) has been removed — the standard 1200 bps touch path is now the single primary trigger.

## Real Promicro-class board result

### What is now working

- first upload from manual bootloader mode works on the user's board
- the board returns to user mode afterward
- with `usbcdc=disabled`, the board keeps a single visible SERVICE CDC path
- UF2 upload from the current DFU port works in both `bootloader=auto` and explicit UF2 menu modes
- explicit Adafruit serial DFU from the current DFU port works and is not confused by a mounted UF2 volume
- with two boards simultaneously mounted as `NICENANO`, the selected board maps to its own drive (`J:` vs `K:` in the hardware run)
- selecting a stale COM after the board re-enumerates is rejected before any upload can target another board
- **a second upload from user mode works** — the 1200 bps touch triggers `NVIC_SystemReset()` into the bootloader, the selected transport streams the image, and the board re-boots into user mode.

### What previously failed (historical)

Before the V1 firmware fixes, `adafruit-nrfutil` would stall at
`Sending DFU start packet` during the second upload because the firmware
never re-entered bootloader in response to the 1200 bps touch. The host then
saw `Port never detached after touch (port stayed present)` repeatedly across
the four host-side trigger mechanisms upload.ps1 used to try.

Root cause: three cooperating firmware bugs in `NrfUsbd.cpp` — EP0 OUT
EasyDMA never triggered, EP0 OUT direction routed by stale `BMREQUESTTYPE`,
and a subsequent DTR=true cancelling `serviceTouchPending_` inside the 40 ms
confirm window.

### `usbcdc=disabled` in-app upload (host-side fix)

For a while, an in-app upload to a `usbcdc=disabled` board stalled at
`adafruit-dfu` even though the firmware touch path was correct. Root cause was
host-side: with no user CDC, the runtime DFU **"Bootloader Control"** vendor
interface lands on `MI_02` (where `usbcdc=enabled` puts the user CDC). `upload.ps1`
counted any non-Ports `MI_02` interface as MSC/bootloader evidence, decided the
board was *already* in the bootloader, **skipped the 1200-touch entirely**, and
ran `adafruit-nrfutil` against the still-running app. The fix excludes the
`Bootloader Control` interface from that evidence (the real bootloader is
identified by its UF2 mass-storage volume, never by this control interface), so
the touch runs and the board reboots normally. Verified 3× back-to-back plus
`usbcdc` transitions in both directions.

### What still needs validation

- Boards beyond the user's ProMicro clone that share the firmware path
- Linux/macOS UF2 parity; those platforms still use the Python/Adafruit serial-DFU recipe
