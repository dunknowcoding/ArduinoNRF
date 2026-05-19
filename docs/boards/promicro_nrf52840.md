# AliExpress ProMicro nRF52840

## Current evidence level

- Pin map: partial
- Battery model: partial
- Upload profile: partial
- LFCLK: `lfxo` with schematic evidence
- Debug access: user-image-backed SWD pads

## Current upload baseline

| Item | Current truth |
|------|---------------|
| Active clone menu under test | `bootloader=promicroserialnosd` |
| Application start | `0x1000` |
| Bootloader family | `0x239A:0x00B3` |
| Upload transport | Adafruit serial DFU through `upload.ps1` |
| Runtime service COM in `promicroserialnosd` | may still appear under the same `0x239A:0x00B3` family |
| Software reset target in code | full bootloader (`GPREGRET = 0x57`) |
| Current status | V1 (button-less reflash on the single SERVICE CDC) is PASSING; see [`../platform/USB_1200_TOUCH_V1_FIX.md`](../platform/USB_1200_TOUCH_V1_FIX.md) |

## Current live-board status

### Verified

- manual bootloader entry on `COM3` can flash the board
- after the first flash, the board returns to user mode
- with `usbcdc=disabled`, the board keeps a single visible SERVICE CDC path
- automatic same-port reupload from user mode works: the 1200 bps touch
  triggers `NVIC_SystemReset()` into the bootloader and the second flash
  completes without manual reset (`verify_promicro_usbcdc_upload_behavior.ps1 -Phase V1` PASS)

### Not yet solved

- dual-CDC real-board closure on this specific clone (V2)
- USB-only GDB-stub real-board closure on this specific clone

## Current package model

- Family: `promicro-compatible`
- Battery sense is modeled on public pin `29`.
- No `EXT_VCC` control pin is modeled.
- No secondary-bus pins are currently modeled for this variant, so `Wire1` and `SPI1` remain unassigned here even though the core exports those global objects.
- SWD is modeled as exposed test pads rather than a populated header.

## Notes

- For `promicroserialnosd`, runtime and bootloader phase detection cannot rely on VID/PID alone.
- The repository no longer claims that this board already has hands-free same-port upload fixed.
- The current source-of-truth progress docs are:
  - `docs\USB_SINGLE_CABLE_PLAN.md`
  - `docs\CURRENT_ACCEPTANCE_REPORT.md`
  - `docs\platform\UPLOAD_BEHAVIOR.md`
