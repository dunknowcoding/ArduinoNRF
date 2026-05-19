# Upload Behavior

Date: 2026-05-18

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
- The wrapper treats same-PID runtime/bootloader cases as a separate class, rather than assuming a visible `0x239A:0x00B3` COM is already a bootloader port. On these boards it does NOT block the DFU on a PnP-level "transition observed" signal (because runtime and bootloader share `0x00B3`) — it surfaces a `[warn] ... Port never detached after touch` informational line and proceeds with a direct DFU attempt, which now succeeds because the firmware-side touch fix lands the chip in the bootloader before the warn fires.
- The previous service-port "boot token" fallback (`~NIUSBL!42\r` after arming with line coding `134/8/2/2 + DTR+RTS`) has been removed — the standard 1200 bps touch path is now the single primary trigger.

## Real Promicro-class board result

### What is now working

- first upload from manual bootloader mode works on the user's board
- the board returns to user mode afterward
- with `usbcdc=disabled`, the board keeps a single visible SERVICE CDC path
- **a second upload from that user-mode `COM3` works** — the 1200 bps touch
  triggers `NVIC_SystemReset()` into the bootloader, adafruit-nrfutil streams
  the image, and the board re-boots into user mode. The full V1 harness
  (`scripts/verify_promicro_usbcdc_upload_behavior.ps1 -Phase V1`) passes
  Pass A + Pass B end to end.

### What previously failed (historical)

Before the V1 firmware fixes, `adafruit-nrfutil` would stall at
`Sending DFU start packet` during the second upload because the firmware
never re-entered bootloader in response to the 1200 bps touch. The host then
saw `Port never detached after touch (port stayed present)` repeatedly across
the four host-side trigger mechanisms upload.ps1 used to try.

Root cause: three cooperating firmware bugs in `NrfUsbd.cpp` — EP0 OUT
EasyDMA never triggered, EP0 OUT direction routed by stale `BMREQUESTTYPE`,
and a subsequent DTR=true cancelling `serviceTouchPending_` inside the 40 ms
confirm window. See [`USB_1200_TOUCH_V1_FIX.md`](USB_1200_TOUCH_V1_FIX.md)
for the full root-cause writeup and the three patches.

### What still needs validation

- V2 (dual-CDC reflash) and V3 (locked OLD-COM cycle) on the user's board
- Boards beyond the user's ProMicro clone that share the firmware path
