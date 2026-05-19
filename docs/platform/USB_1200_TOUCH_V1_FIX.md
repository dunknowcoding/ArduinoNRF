# USB 1200 bps Touch — V1 Fix (ProMicro nRF52840 single SERVICE CDC)

This document is the authoritative writeup of why the button-less reflash
chain (V1: two `compile+upload` cycles on the SAME SERVICE CDC without a
manual reset) was broken on the AliExpress ProMicro nRF52840 clone, and the
three firmware patches that fix it. It supersedes the older descriptions in
`VALIDATION_GUIDE.md` / `README_VALIDATION.md` that called V1 a blocker.

## Configuration in scope

```text
board:      arduinonrf:nrf52:promicro_nrf52840
options:    bootloader=promicroserialnosd,usbcdc=disabled,usbdesc=no_app_dfu
identity:   VID 0x239A, PID 0x00B3 (shared by runtime + bootloader)
descriptor: single SERVICE CDC interface (MI_00). No user CDC, no MSC, no DFU IAD in runtime.
```

In this build the SERVICE CDC carries both the user's `Serial` output and the
host's 1200 bps touch that drives bootloader entry.

## Observed failure before the fix

1. Pass A (first flash after manual reset to bootloader) — OK.
2. Board returns to user mode; `COM3` remains visible.
3. Pass B (second flash from user mode) — `upload.ps1` runs its full trigger
   sequence on `COM3` and every attempt times out with
   `Port never detached after touch (port stayed present)`. The script falls
   through to a direct DFU attempt, adafruit-nrfutil opens `COM3`, prints
   `Sending DFU start packet`, and stalls indefinitely.
4. The MCU stays in user mode for the entire sequence — a serial monitor on
   `COM3` shows `tick` lines streaming throughout the touch attempts.

## Root cause — three cooperating bugs in `NrfUsbd.cpp`

### Bug 1 — EP0 OUT EasyDMA was never triggered

`EVENTS_EP0DATADONE` on the nRF52840 USBD signals that the host→device DATA
packet has been accepted into the peripheral's internal buffer, **not** that
the bytes are in RAM. Per nRF52840 Product Spec § 6.36 (USBD EasyDMA model),
the firmware must trigger `TASKS_STARTEPOUT[0]` after `EP0DATADONE` and wait
for `EVENTS_ENDEPOUT[0]`. Only then does `EPOUT[0].AMOUNT` reflect the actual
byte count and the buffer pointed to by `EPOUT[0].PTR` contain the data.

The previous handler called `completeControlOutTransfer` immediately on
`EP0DATADONE` with no `TASKS_STARTEPOUT[0]`. `controlOutLength_` therefore
read 0, the `if (controlOutLength_ >= 7U)` guard inside the
`ServiceLineCoding` branch skipped the line-coding decode, and every host
`SET_LINE_CODING(1200)` was silently discarded. The device's
`lineCoding_.baudRate` stayed at its default initializer
(`NrfUsbd.h: {115200, 0, 0, 8}`), so the touch handler's
`lineCoding_.baudRate == 1200UL` gate never matched and `serviceTouchPending_`
never armed — no matter which host mechanism (1200 touch, 115200→1200 touch,
service-port boot token) the script tried.

**Fix** ([NrfUsbd.cpp `processBusState`](../../hardware/arduinonrf/nrf52/cores/arduino/NrfUsbd.cpp)):
trigger `TASKS_STARTEPOUT[0]` on `EP0DATADONE` for the OUT direction and
spin-wait briefly for `EVENTS_ENDEPOUT[0]` before invoking
`completeControlOutTransfer`.

### Bug 2 — EP0 OUT direction was decided from a stale `BMREQUESTTYPE`

The previous EP0DATADONE handler read `BMREQUESTTYPE` from the USBD register
and routed by its direction bit (bit 7) — IN to `sendEp0ControlInChunkOrAdvanceStatus`,
OUT to `completeControlOutTransfer`. But `BMREQUESTTYPE` is volatile: a
follow-up SETUP transaction (Windows routinely issues GET_LINE_CODING right
after SET_LINE_CODING to verify the result) overwrites the register before
the firmware poll observes the SET_LINE_CODING DATADONE. The handler then
read direction=IN and skipped the OUT-side path entirely; even if EasyDMA had
run, the payload would still have been dropped.

**Fix** ([NrfUsbd.cpp `processBusState`](../../hardware/arduinonrf/nrf52/cores/arduino/NrfUsbd.cpp)):
route by internal state (`ep0InXferPhase_` and `pendingControlOut_`) which is
set when the transfer is initiated, not by the volatile peripheral register.

### Bug 3 — DTR=true cancelled the touch before the 40 ms confirm window

`upload.ps1`'s touch opens the port at 1200 baud, drops DTR, waits ~100 ms,
then closes. Windows usbser.sys re-asserts DTR on `SerialPort.Close()` and
again on the next `CreateFile()` (adafruit-nrfutil opens the port immediately
to start DFU). The firmware was treating the second DTR=true CDC line-state
update as a touch cancellation: the `else if (dtr)` branch unconditionally
cleared `serviceTouchPending_`. Even when bugs 1+2 were fixed and the touch
finally armed, the host-side re-assert cleared the pending state inside the
40 ms confirm window before the poll loop could fire `requestBootloaderReset`.

**Fix** ([NrfUsbd.cpp `handleClassRequest`](../../hardware/arduinonrf/nrf52/cores/arduino/NrfUsbd.cpp)
plus the matching poll/IRQ gate): once the confirm window has started
(`serviceTouchResetMillis_ != 0UL`), the touch is sticky — subsequent
DTR=true line-state transitions do NOT cancel it, and the gate in
poll/`irqHandler` tolerates `dtr_ == true` for the `USBD_TOUCH_RESET_CONFIRM_MS`
(40 ms) window so the confirm path runs to completion.

## What was removed

The service-port boot-token workaround
(`~NIUSBL!42\r` written on the SERVICE CDC after arming with line coding
`baud=134, stop=2, parity=2, dataBits=7` plus DTR+RTS) was added as a
fallback while the 1200 bps touch was broken. With the three fixes above,
the standard 1200 bps touch works reliably and the token mechanism is
redundant. Both the firmware-side matcher (constants, ring window,
`armServiceBootTokenWindow` / `clearServiceBootTokenWindow` /
`flushServiceBootTokenMatch`, diag-cause codes) and the host-side helpers in
`upload.ps1` (`Invoke-ServicePortBootToken`,
`Invoke-ServicePortBootTokenTransition`, token-byte parsers, environment
override `NIUS_SERVICE_BOOT_TOKEN`) have been removed.

## What to watch for in future regressions

- If V1 starts failing again with `Port never detached after touch`, attach a
  serial monitor to `COM3` from a Python pyserial script that captures the
  full host→device line-coding round-trip. The fastest signal is whether
  `lineCoding_.baudRate` actually becomes `1200` on the device — that one
  bit gates everything downstream.
- The `same-PID detection` warnings printed by `upload.ps1` (e.g.
  `[warn] ... touch on COMx did not produce a confirmed bootloader transition`)
  are EXPECTED on this board because runtime + bootloader share PID 0x00B3,
  so the host cannot see a PnP-level transition. The DFU then succeeds via
  the direct-DFU fallback. Treat that warning as informational unless the
  DFU itself also fails.
- Do NOT set `boards.use_1200bps_touch=true`. arduino-cli's native touch
  races our PowerShell open sequence on Windows; `upload.ps1` owns the touch
  via `-UseTouch1200 true` so the timing is deterministic.

## Verification

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify_promicro_usbcdc_upload_behavior.ps1 `
  -Phase V1 -UseArduinoCiConfig -Port COM3
```

Expected: `[verify] V1 PASS`. Both compile+upload cycles return exit 0 and
the harness asserts exactly one runtime COM after Pass B.
