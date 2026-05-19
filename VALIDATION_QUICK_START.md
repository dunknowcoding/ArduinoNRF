# ProMicro nRF52840 Quick Start

## Current reality

V1 (button-less reflash on a single SERVICE CDC) is now passing end to end.
The board can be reflashed twice on the same COM (e.g. `COM3`) without any
manual reset between flashes.

The historical failure — `adafruit-nrfutil` stalling at
`Sending DFU start packet` because the 1200 bps touch never triggered the
firmware-side bootloader reset — is fixed. See
[`docs/platform/USB_1200_TOUCH_V1_FIX.md`](docs/platform/USB_1200_TOUCH_V1_FIX.md)
for the three coordinated firmware patches.

## Best current smoke test

### 1. Put the board in bootloader mode manually (first flash only)

Short `RST` to `GND` twice. UF2 drive appears; `COM3` should be visible.

### 2. Flash the minimal single-COM test image

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\hardware_upload_minimal_usb.ps1 `
  -Port COM3 -BootloaderMenu promicroserialnosd -UseArduinoCiConfig -MinimalUsb
```

### 3. Confirm the board comes back in user mode

Expected good result:

- `COM3` still exists
- a serial monitor on `COM3` shows `MinimalUsbSmoke: setup OK ...` and `tick` lines

### 4. Run the full V1 harness (two compile+upload cycles on the same COM)

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify_promicro_usbcdc_upload_behavior.ps1 `
  -Phase V1 -UseArduinoCiConfig -Port COM3
```

### 5. Interpret the result

Expected good output near the end:

```text
[verify] V1-3/4: Pass B - second compile+upload (same COM, same -MinimalUsb)...
[verify] <<< DONE hardware_upload exit=0
[verify] V1-4/4: assert exactly one runtime COM after Pass B...
[verify] V1 PASS
```

If you instead see `adafruit-nrfutil` stalling at `Sending DFU start packet`
during Pass B, the firmware-side V1 fix may have regressed — check
[`docs/platform/USB_1200_TOUCH_V1_FIX.md`](docs/platform/USB_1200_TOUCH_V1_FIX.md)
against the current `NrfUsbd.cpp` state.

## Read next

- [`docs/platform/USB_1200_TOUCH_V1_FIX.md`](docs/platform/USB_1200_TOUCH_V1_FIX.md) — what was broken and how the three patches fix it
- [`docs/USB_SINGLE_CABLE_PLAN.md`](docs/USB_SINGLE_CABLE_PLAN.md)
- [`docs/platform/UPLOAD_BEHAVIOR.md`](docs/platform/UPLOAD_BEHAVIOR.md)
- [`docs/boards/promicro_nrf52840.md`](docs/boards/promicro_nrf52840.md)
