# Arduino nRF52840 ProMicro Validation Guide

## Current hardware truth

- **Board**: AliExpress ProMicro nRF52840 clone
- **Primary bootloader family**: `0x239A:0x00B3`
- **Single-COM build under validation**: `bootloader=promicroserialnosd,usbcdc=disabled,usbdesc=no_app_dfu`
- **Application start**: `0x1000`
- **Upload transport**: Adafruit serial DFU through `upload.ps1`

In this configuration the runtime SERVICE CDC and the bootloader share the same
VID:PID family (`0x239A:0x00B3`), so VID/PID alone cannot tell host-side
tooling which mode the board is in. The current upload pipeline copes via
same-PID-aware detection in `upload.ps1`.

## V1: button-less reflash on a single SERVICE CDC — PASSING

The button-less reflash workflow that V1 enforces is now working end to end:

1. Pass A — `arduino-cli compile + upload` succeeds on the SERVICE CDC (e.g. `COM3`).
2. The board returns to user mode; `COM3` remains visible as the SERVICE CDC.
3. Pass B — a SECOND `compile + upload` on the SAME `COM3` succeeds without
   any manual reset. `upload.ps1` sends the 1200 bps + DTR-drop touch, the
   firmware confirms within the 40 ms window, GPREGRET is set to
   `kAdafruitUf2ResetMagic` (0x57), `NVIC_SystemReset()` jumps to the
   bootloader, adafruit-nrfutil streams the new image, and the board re-boots
   into user mode.

The V1 fix lives entirely on the device side, in three coordinated patches in
[`NrfUsbd.cpp`](hardware/arduinonrf/nrf52/cores/arduino/NrfUsbd.cpp). See
[`docs/platform/USB_1200_TOUCH_V1_FIX.md`](docs/platform/USB_1200_TOUCH_V1_FIX.md)
for the full root-cause writeup and the historical failure modes (EP0 OUT
EasyDMA never triggered, EP0 DATADONE routed by stale `BMREQUESTTYPE`, host
DTR re-assert cancelled the touch before the confirm window elapsed).

## Recommended commands

### Compile

```powershell
arduino-cli compile --config-file .arduino-ci.yaml `
  --fqbn "arduinonrf:nrf52:promicro_nrf52840:bootloader=promicroserialnosd,usbcdc=disabled,usbdesc=no_app_dfu" `
  examples\MinimalUsbSmoke
```

### First flash from manual bootloader mode

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\hardware_upload_minimal_usb.ps1 `
  -Port COM3 -BootloaderMenu promicroserialnosd -UseArduinoCiConfig -MinimalUsb
```

### End-to-end V1 check (compile + upload twice on the SAME COM, asserts a single COM after Pass B)

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify_promicro_usbcdc_upload_behavior.ps1 `
  -Phase V1 -UseArduinoCiConfig -Port COM3
```

Expected tail of the run:

```text
[verify] V1-3/4: Pass B - second compile+upload (same COM, same -MinimalUsb)...
[verify] <<< DONE hardware_upload exit=0
[verify] V1-4/4: assert exactly one runtime COM after Pass B...
[verify] V1 PASS
[verify] STATE after V1: usbcdc=DISABLED, user mode, single COM (USB Serial DFU chain OK).
```

After V1 the board sits in user mode with a single SERVICE CDC. V2/V3 can be
run from there to validate the dual-CDC and OLD-COM-lockdown scenarios.

## Operator notes

- The board has no physical reset button; the only manual recovery is shorting
  the `RST` pin to `GND` twice. The V1 chain exists precisely so this manual
  step is not required for routine reflashes.
- Same-PID detection: upload.ps1 may print
  `[warn] ... touch on COMx did not produce a confirmed bootloader transition`
  even when the touch succeeded, because runtime + bootloader share PID
  `0x00B3` and the host cannot see a PnP transition. The DFU then succeeds via
  the direct-DFU fallback; that warning is informational, not a failure.
- Do NOT set `boards.use_1200bps_touch=true` for `promicro_nrf52840`. The
  Arduino-CLI native touch races our PowerShell open sequence on Windows;
  `upload.ps1` owns the touch via `-UseTouch1200 true`.

## Source-of-truth docs

- [`docs/platform/USB_1200_TOUCH_V1_FIX.md`](docs/platform/USB_1200_TOUCH_V1_FIX.md) — the V1 root cause and the three firmware fixes
- [`docs/USB_SINGLE_CABLE_PLAN.md`](docs/USB_SINGLE_CABLE_PLAN.md)
- [`docs/CURRENT_ACCEPTANCE_REPORT.md`](docs/CURRENT_ACCEPTANCE_REPORT.md)
- [`docs/platform/UPLOAD_BEHAVIOR.md`](docs/platform/UPLOAD_BEHAVIOR.md)
- [`docs/boards/promicro_nrf52840.md`](docs/boards/promicro_nrf52840.md)
