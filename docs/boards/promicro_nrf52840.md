# AliExpress ProMicro nRF52840

## Current evidence level

- Pin map: partial
- Battery model: partial
- Upload profile: verified on hardware for `bootloader=promicroserialnosd,usbcdc=enabled`
- LFCLK: `lfxo` with schematic evidence
- Debug access: user-image-backed SWD pads
- Single-cable USB-CDC GDB stub: verified on hardware

## Current upload baseline

| Item | Current truth |
| ------ | --------------- |
| Active clone menu under test | `bootloader=promicroserialnosd` |
| Application start | `0x1000` |
| Bootloader family | `0x239A:0x00B3` |
| Upload transport | Adafruit serial DFU through `upload.ps1` |
| Runtime service COM in `promicroserialnosd` | may still appear under the same `0x239A:0x00B3` family |
| Software reset target in code | full bootloader (`GPREGRET = 0x57`) |
| Current status | V1 hands-free reflash and USB-CDC GDB-stub debug are both PASSING; see [`../VALIDATION.md`](../VALIDATION.md) and [`../platform/USB_1200_TOUCH_V1_FIX.md`](../platform/USB_1200_TOUCH_V1_FIX.md) |

## Current live-board status

### Verified

- manual bootloader entry on `COM3` can flash the board
- after the first flash, the board returns to user mode
- with `usbcdc=enabled`, the board exposes separate user and service CDC paths
- selecting the user CDC for upload is rejected; the service CDC hands-free reupload path works repeatedly without manual reset
- with `usbcdc=disabled`, the board remains bootloader-uploadable but in-app 1200-touch is not the recommended workflow
- USB-only GDB-stub debug over the service CDC works with breakpoints, step, registers, memory, watchpoints, and pause

### Still board-specific

- For `promicroserialnosd`, runtime and bootloader phase detection cannot rely on VID/PID alone.
- No variant-level secondary-bus pins are modeled for this board, so `Wire1` and `SPI1` stay unassigned here even though the core exports those global objects.

## Current package model

- Family: `promicro-compatible`
- Battery sense is modeled on public pin `29`.
- No `EXT_VCC` control pin is modeled.
- SWD is modeled as exposed test pads rather than a populated header.

## Notes

- The current source-of-truth docs are:
  - `docs/VALIDATION.md`
  - `docs/platform/UPLOAD_BEHAVIOR.md`
  - `docs/platform/USB_1200_TOUCH_V1_FIX.md`
