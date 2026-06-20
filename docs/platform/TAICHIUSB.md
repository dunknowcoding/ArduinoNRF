# TaichiUSB — the ArduinoNRF USB device stack

**TaichiUSB** is ArduinoNRF's clean-room USB device stack for the nRF52840. It is
written directly against the nRF `USBD` peripheral and is **not** TinyUSB — it
shares no code with it. It powers the user `Serial` (USB-CDC), the
maintenance/upload CDC, the 1200-bps-touch hands-free upload handoff, and the
single-cable USB GDB stub.

## Why a self-developed stack

- **Enumeration is serviced from the USBD ISR**, so it keeps running regardless
  of what the user sketch's `setup()`/`loop()` does (a forever-blocking `setup()`
  still enumerates a COM port). The same routines are re-entrant from the
  foreground `poll()` path used by `yield()`.
- Tight control of the nRF-specific bring-up: the errata-187/171-wrapped `ENABLE`
  handshake, `VBUS`/`OUTPUTRDY` sequencing, bootloader→app hand-off recovery, and
  the dual-CDC (user + maintenance) composite device.

## Source layout

| File | Role |
|------|------|
| `cores/arduino/TaichiUsb.h` | Stack identity, version, **build-flag guard**, public facade/aliases |
| `cores/arduino/NrfUsbd.{h,cpp}` | Device core: endpoints (EasyDMA), control transfers, suspend/resume, ISR |
| `cores/arduino/NrfUsbSerial.cpp` | User CDC (`Serial`) |
| `cores/arduino/NrfServiceSerial.cpp` | Maintenance/upload CDC + 1200-touch |
| `cores/arduino/NrfGdbStub.{h,cpp}` | **Debug submodule**: single-cable GDB stub over the service CDC |

## Debug submodule: the USB GDB stub

The single-cable GDB stub (`NrfGdbStub`) is a **submodule of TaichiUSB**, not a
separate stack: its Remote-Serial-Protocol transport is bound to TaichiUSB's
maintenance/service CDC, and it leans on a small set of stub-halted hooks the
device core exposes specifically for it:

- `NrfUsbdDriver::setStubHalted()` — tells the driver it is being pumped from the
  halted DebugMon loop (so the 1200-touch uses a poll-counter, not frozen
  `millis()`, and an upload to a halted board can't brick flash).
- `serviceHaltedTouch()` / `kickServiceDataIn()` / `drainServiceDataOut()` /
  `armCdcDataOut()` — let the stub move GDB packets over the service CDC while the
  application (and SysTick) are stopped, since this silicon does not raise
  `EVENTS_EPDATA` reliably for received OUT packets.

So debugging rides the *same* enumeration-from-ISR USB core as `Serial` and the
uploader; there is no second USB implementation. Application `Serial` (user CDC)
stays separate — don't multiplex sketch `printf` onto the service CDC during a
debug session.

## Build-flag guard (important)

On this core the per-board **`build.extra_flags` is an aggregate** that carries
the critical system defines:

```
build.extra_flags = {build.base_flags} {build.system_flags}
                    {build.usb_backend_flags} {build.usb_cdc_flags} ...
```

`-DNRF_SYSTEM_HAS_USB_CDC=0|1` comes from the `usbcdc` menu via
`build.usb_cdc_flags`. If a build **overrides `build.extra_flags` wholesale**
(for example `arduino-cli --build-property build.extra_flags=-DMY_DEFINE`), all of
those defines are dropped and USB silently compiles itself out — the firmware
runs but never enumerates a COM port.

**Add custom defines via `compiler.cpp.extra_flags` / `compiler.c.extra_flags`
instead** (they are empty by default and are threaded through the compile recipe
separately), e.g.:

```
arduino-cli compile --fqbn arduinonrf:nrf52:promicro_nrf52840 \
  --build-property "compiler.cpp.extra_flags=-DMY_DEFINE=1" <sketch>
```

`TaichiUsb.h` now contains a **compile-time guard** that turns this
misconfiguration into a clear error (anchored on `NRF52_SERIES`, which is *not*
part of `build.extra_flags`) instead of a silently USB-less binary.
