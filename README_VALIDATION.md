# Arduino nRF52840 ProMicro Validation Status

## Current status

V1 (button-less reflash on a single SERVICE CDC) is **passing** on the
reference AliExpress ProMicro nRF52840 clone with the build
`bootloader=promicroserialnosd,usbcdc=disabled,usbdesc=no_app_dfu`:

- manual bootloader entry on `COM3` flashes the user firmware
- the board returns to user mode and keeps `COM3` as the single SERVICE CDC
- a SECOND `arduino-cli compile + upload` on the SAME `COM3` succeeds with no
  manual reset — the 1200 bps touch triggers `NVIC_SystemReset()` into the
  bootloader, adafruit-nrfutil streams the new image, and the board re-boots
  into user mode

V2 (dual-CDC scenario) and V3 (OLD-COM lockdown) can be run from there.

## What changed since the older "5/5 validation pass" wording

The historical failure mode — the host sends the 1200 bps + DTR-drop touch
but Windows never observes a USB detach and `adafruit-nrfutil` stalls at
`Sending DFU start packet` — was caused by three cooperating firmware bugs
in [`NrfUsbd.cpp`](hardware/arduinonrf/nrf52/cores/arduino/NrfUsbd.cpp):

1. **EP0 OUT EasyDMA was never triggered.** `EVENTS_EP0DATADONE` only signals
   that host→device data is in the peripheral's internal buffer; the firmware
   must fire `TASKS_STARTEPOUT[0]` and wait for `EVENTS_ENDEPOUT[0]` before
   `EPOUT[0].AMOUNT` is non-zero. Without that step,
   `completeControlOutTransfer` read length 0, the `if (controlOutLength_ >= 7U)`
   guard dropped every SET_LINE_CODING payload, and the device's
   `lineCoding_.baudRate` stayed at its 115200 default initializer.
2. **EP0 OUT routing used the volatile `BMREQUESTTYPE` register**, which a
   follow-up SETUP transaction (e.g. GET_LINE_CODING right after
   SET_LINE_CODING) can overwrite before the firmware processes the DONE
   event. A late OUT completion then took the IN-side branch and the payload
   was silently dropped.
3. **A subsequent `DTR=true` cancelled `serviceTouchPending_`** before the
   40 ms confirm window elapsed. `.NET SerialPort.Close()` + the next
   `CreateFile()` from adafruit-nrfutil re-asserts DTR within tens of ms,
   which is exactly the situation the touch needs to ignore.

The fixes are detailed in
[`docs/platform/USB_1200_TOUCH_V1_FIX.md`](docs/platform/USB_1200_TOUCH_V1_FIX.md).
The previously-coded service-port "boot token" workaround (a magic byte
sequence `~NIUSBL!42\r` written on the SERVICE CDC after a 134/8/2/2 line
coding arm) is no longer needed and has been removed from both the firmware
and `upload.ps1`.

## Current recommended validation path

Run the harness directly:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify_promicro_usbcdc_upload_behavior.ps1 `
  -Phase V1 -UseArduinoCiConfig -Port COM3
```

Pass A + Pass B both exit 0 and the harness asserts a single runtime COM
after Pass B. After V1, the board is in user mode and `-Phase V2` / `-Phase V3`
can be run.

## Key files

- [`hardware/arduinonrf/nrf52/cores/arduino/NrfUsbd.cpp`](hardware/arduinonrf/nrf52/cores/arduino/NrfUsbd.cpp) — V1 fixes (EP0 EasyDMA trigger, EP0 routing, DTR latch)
- [`hardware/arduinonrf/nrf52/tools/niusrobotlab/upload.ps1`](hardware/arduinonrf/nrf52/tools/niusrobotlab/upload.ps1) — same-PID upload pipeline
- [`scripts/hardware_upload_minimal_usb.ps1`](scripts/hardware_upload_minimal_usb.ps1) — single iteration wrapper
- [`scripts/verify_promicro_usbcdc_upload_behavior.ps1`](scripts/verify_promicro_usbcdc_upload_behavior.ps1) — V1/V2/V3 harness
- [`docs/platform/USB_1200_TOUCH_V1_FIX.md`](docs/platform/USB_1200_TOUCH_V1_FIX.md) — root-cause writeup

## Historical scripts

`validation.bat` and `validation_script.py` predate the same-PID
runtime/bootloader behavior and the V1 fix described above. Treat them as
historical references; the harness above is the authoritative path.
