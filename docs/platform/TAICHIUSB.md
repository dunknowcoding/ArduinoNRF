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

## Startup and enumeration contract

TaichiUSB follows the nRF52840 power-up order: prove the selected board's VBUS
contract, disconnect and confirm the previous controller is disabled, hold the
host detach interval, enable the controller inside fresh Errata 187/171 brackets,
observe both controller `READY` and regulator `OUTPUTRDY`, and only then connect
D+. Startup is a bounded, nonblocking state machine advanced from both SysTick
and foreground `yield()`/`poll()`. A cable removed during the sequence leaves
the request pending; a later insertion restarts it even when application code
never yields. TaichiUSB does not claim PendSV or POWER/CLOCK IRQ ownership to
obtain that guarantee. SysTick runs below the USBD IRQ, observes VBUS at 1 kHz,
and retires a disconnected controller without preempting an active USB handler.
Because the higher-priority USBD IRQ could otherwise preempt that lower-priority
tick while both own lifecycle/touch state, the tick masks only USBD around its
bounded shared-state step. Startup uses a core interrupt guard for the final
IRQ-enable/pull-up commit.
The pull-up and controller lifecycle use the nRF POWER block's real
`USBREGSTATUS.VBUSDETECT`; board profiles cannot substitute an assumed VBUS
level, so battery-only execution cannot expose a stale USB session.

The stack makes one bounded, fully bracketed retry when the controller does not
acknowledge `READY`. If the second attempt, HFCLK, regulator, or disable readback
still fails while VBUS remains present, the driver stays detached until chip
reset. It never reports `ready()` or exposes a pull-up merely because a wait
expired. Once this startup contract completes, EP0 and CDC enumeration progress
in the USBD interrupt and do not depend on the sketch loop.

`begin()` and an already-live `attach()` are idempotent and serialized against
the SysTick startup service. Repeating either call cannot silently clear an
active USB configuration while the pull-up remains visible to the host.
Foreground service locks also preserve a VBUS-loss shutdown: they restore the
USBD NVIC route only while the peripheral interrupt mask is still owned.

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
- `serviceHaltedTouch()` / `kickServiceDataIn()` / `drainServiceDataOut()` let
  the stub move GDB packets over the service CDC while the application (and
  SysTick) are stopped. OUT readiness is taken from `EPDATASTATUS`; packet size
  is read from `SIZE.EPOUT` before the shared EasyDMA engine is armed.

EP0, both CDC functions, notifications, and optional dynamic endpoints share
one serialized EasyDMA arbiter. OUT endpoints are opened once and re-armed by
the peripheral after a completed transfer; IN endpoints remain busy until the
host ACK is observed in `EPDATASTATUS`. This prevents control/data overlap,
premature buffer reuse, and packet loss under simultaneous upload, logging, and
debug traffic.
Bulk OUT backpressure uses the exact bounded `SIZE.EPOUT` packet length. A short
packet or zero-length packet is therefore drained as soon as its own bytes fit,
without waiting for an unnecessary full 64-byte ring slot.

Suspend/resume state is applied before EP0 or data-endpoint events from the same
interrupt snapshot. While suspended, only USB reset and USB-event wake sources
remain armed; transfer events stay latched and are serviced after resume. This
prevents an interrupt storm and prevents stale active state from launching new
EasyDMA work during bus suspend.

Pluggable modules are admitted transactionally. Registration first rejects an
endpoint/interface allocation beyond the nRF52840 controller limits. During
descriptor construction, each module must write exactly the byte and interface
counts it reports without exceeding the composite buffer; otherwise its partial
descriptor is rolled back, it receives no setup/data callbacks, and the next
valid module reuses the interface number. An invalid optional module therefore
cannot truncate the configuration header or prevent the maintenance CDC from
enumerating.

The same transaction rule applies after enumeration. A module that owns a
control-IN request must append exactly the positive byte count it reports. A
zero return must append nothing. Negative, overflowed, or mismatched responses
are rolled back and stalled, and dispatch stops at the first valid owner instead
of concatenating two modules' replies or exposing stale EP0 buffer bytes.
The current boolean `setup()` callback owns metadata-only control-OUT requests;
nonzero OUT payloads and nonexistent interface recipients stall until a bounded
payload/completion API is explicitly implemented.

A newer SETUP token aborts the previous control transfer before any old payload
or status completion is committed. If SETUP and `EP0DATADONE` are observed
together, the completion belongs to the aborted request and is discarded before
the new request is admitted. An aborted CDC line-coding write therefore cannot
leave 1200-baud upload authority behind for a later DTR transition.
`SET_CONFIGURATION` and `GET_CONFIGURATION` are admitted only after a completed
nonzero `SET_ADDRESS`; malformed default-state requests cannot create endpoint
or function-session state.

So debugging rides the *same* enumeration-from-ISR USB core as `Serial` and the
uploader; there is no second USB implementation. Application `Serial` (user CDC)
stays separate — don't multiplex sketch `printf` onto the service CDC during a
debug session.

## CDC throughput (block write)

`Serial.write(buffer, size)` takes the **block-write fast path**: the whole buffer
is pushed to the user-CDC TX ring lock-free (the ring is single-producer /
single-consumer) and the IN endpoint is armed **once**. The earlier path armed it
*per byte*, taking a `UsbdIrqLock` (which masks the USBD interrupt) around
`serviceDataIn()` 64 times for a 64-byte write — and that foreground lock churn
starved the EPDATA ISR that arms the next packet, leaving a NAK gap per packet.

Measured device->host CDC throughput on a ProMicro nRF52840 (pyserial reader):

| TX path | KB/s |
| --- | --- |
| per-byte arm (before) | ~23 |
| block write (after) | ~304 |

~13x faster, with the byte stream verified intact (no corruption; drop-on-full
semantics are unchanged, just far rarer now that the ring drains ~13x faster).
For sustained bulk transfers, use a host reader that issues large back-to-back
reads - slow per-byte host tools (e.g. some serial monitors) cap well below this.

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
