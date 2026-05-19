# Release Notes — v0.1.0

First public release of the `arduinonrf:nrf52` Arduino board library.

Install in Arduino IDE 2.x by adding this URL to
**File → Preferences → Additional Boards Manager URLs**:

```
https://raw.githubusercontent.com/dunknowcoding/ArduinoNRF/main/package_arduinonrf_index.json
```

Then open **Tools → Board → Boards Manager**, search for "Arduino NRF52
Boards", and click Install.

## Highlights

### Button-less reflash works (V1 / V2 / V3)

On the AliExpress ProMicro nRF52840 clone with the `bootloader=promicroserialnosd`
menu, two consecutive `arduino-cli compile + upload` cycles now succeed on
the same single SERVICE-CDC COM (e.g. `COM3`) without any manual
RST→GND reset between flashes. This is the V1 acceptance criterion and
was historically the hardest blocker on this clone family because the
runtime and bootloader share VID/PID `0x239A:0x00B3`, so Windows cannot
detect a bootloader transition from PnP alone.

The fix lives entirely on the device side, in three coordinated patches
in [hardware/arduinonrf/nrf52/cores/arduino/NrfUsbd.cpp](../../hardware/arduinonrf/nrf52/cores/arduino/NrfUsbd.cpp).
See [docs/platform/USB_1200_TOUCH_V1_FIX.md](../platform/USB_1200_TOUCH_V1_FIX.md)
for the full root-cause writeup. In short, on the nRF52840 USBD:

1. **EP0 OUT EasyDMA was never triggered after `EVENTS_EP0DATADONE`.**
   That event only signals that the host→device data is in the
   peripheral's internal buffer; the firmware must also fire
   `TASKS_STARTEPOUT[0]` and wait for `EVENTS_ENDEPOUT[0]` before the
   bytes are in RAM and `EPOUT[0].AMOUNT` reflects the byte count.
   Without that step the `controlOutLength_ >= 7U` guard inside
   `completeControlOutTransfer` dropped every host
   `SET_LINE_CODING(1200)`, and the touch gate (`baudRate == 1200`)
   never armed.
2. **EP0 OUT direction was decided from a stale `BMREQUESTTYPE` register.**
   Follow-up SETUPs from Windows (e.g. GET_LINE_CODING immediately after
   SET_LINE_CODING) overwrite the register before poll observes the
   OUT data done event, so the previous handler routed OUT completions
   to the IN-side branch.
3. **DTR=true cancelled the touch before the 40 ms confirm window.**
   Windows usbser.sys re-asserts DTR on `SerialPort.Close()` and again
   on the next `CreateFile()` within tens of ms. The firmware now
   latches the pending touch once the confirm timer has started and
   ignores subsequent DTR=true transitions for the window duration.

V2 also passes (uploading to the USER CDC port in dual-CDC mode is
correctly rejected with a clear guidance message), and V3 passes (flash
CDC-enabled then CDC-disabled images back-to-back on the OLD/service
COM).

### Supported boards

- AliExpress ProMicro nRF52840
- nice!nano v2
- SuperMini nRF52840
- nRFMicro nRF52840
- Mini nRF52840
- XIAO-like nRF52840
- Generic nRF52840 Development Board
- Generic nRF52833 Development Board
- Pitaya Go nRF52840
- nRF52840 USB Dongle

The reference / most-tested board is **AliExpress ProMicro nRF52840** with
the `bootloader=promicroserialnosd` menu (single SERVICE CDC, app start
`0x1000`).

### Tool dependencies

Inherited from the upstream `arduino` package:

- `arm-none-eabi-gcc` 7-2017q4
- `dfu-util` 0.10.0-arduino1
- `openocd` 0.11.0-arduino2

Arduino IDE 2.x Boards Manager fetches these automatically when you
install the platform.

### USB CDC GDB stub (preview)

A USB-CDC GDB stub is included for IDE 2.x debug under the
`buildprofile=usbgdbstub` menu option. It supports hardware breakpoints
via Cortex-M4 FPB (6 simultaneous), single-step via DEMCR.MON_STEP, and
HardFault/MemManage/BusFault/UsageFault interception. See
[docs/platform/ARDUINO_IDE2_USB_GDBSTUB.md](../platform/ARDUINO_IDE2_USB_GDBSTUB.md)
for the current debug workflow. End-to-end IDE 2 Debug-button integration
(auto-launch the host bridge, debug+upload coexistence) is in progress
for a follow-up release.

## Verification

The three repeatable acceptance tests are driven by
`scripts/verify_promicro_usbcdc_upload_behavior.ps1`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts\verify_promicro_usbcdc_upload_behavior.ps1 `
  -Phase All -UseArduinoCiConfig -Port COM3
```

Expected: `[verify] V1 PASS`, `[verify] V2 PASS`, `[verify] V3 PASS`.
