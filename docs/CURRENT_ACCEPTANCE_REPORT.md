# Current Acceptance Report

Date: 2026-05-19

## Scope of this snapshot

Real-board status for the user's AliExpress `promicro_nrf52840` clone after
the V1 button-less reflash chain was fixed and validated end to end.

The previously-documented blocker — `adafruit-nrfutil` stalling at
`Sending DFU start packet` during the SECOND upload because the firmware
never re-entered bootloader in response to the 1200 bps touch — is resolved.
See [`platform/USB_1200_TOUCH_V1_FIX.md`](platform/USB_1200_TOUCH_V1_FIX.md)
for the root cause and the three firmware patches.

## Verified on the user's board

### V1 — button-less reflash on a single SERVICE CDC

`scripts/verify_promicro_usbcdc_upload_behavior.ps1 -Phase V1` passes end to
end:

- Pass A: `arduino-cli compile + upload` on `COM3` exits 0
- USB settle, no manual reset
- Pass B: a SECOND `compile + upload` on the SAME `COM3` exits 0
- Final assert: exactly one runtime COM present after Pass B

The board ends in user mode with a single SERVICE CDC visible on `COM3`.

### First-flash path (manual bootloader entry)

- Short `RST` to `GND` twice → UF2 drive appears, `COM3` is visible
- `hardware_upload_minimal_usb.ps1 -BootloaderMenu promicroserialnosd -MinimalUsb`
  flashes the user firmware
- Board returns to user mode; `COM3` remains visible

### User-mode runtime after the flash

- `usbcdc=disabled` exposes only the SERVICE CDC (single COM)
- Arduino `Serial` is routed to that SERVICE CDC, so sketches and the IDE
  serial monitor work on the same cable used for reflash
- The same single COM is what `upload.ps1` uses for the 1200 bps touch on
  the next reflash

## Important implementation state in the repository

- `promicroserialnosd` is the active clone-focused menu under test.
- Runtime + bootloader intentionally share VID:PID `0x239A:0x00B3`, so
  host-side PnP cannot detect the runtime→bootloader transition by PID. The
  upload pipeline copes with `runtimeSharesUploadIdentity` plumbing in
  `upload.ps1` and a direct-DFU fallback that succeeds even when the host
  reports `Port never detached after touch` (the touch DID work; the host
  cannot see the transition).
- The firmware-side touch path now correctly DMAs SET_LINE_CODING into RAM,
  routes EP0 OUT data done by internal state (not the volatile
  `BMREQUESTTYPE` register), and latches `serviceTouchPending_` once the
  40 ms confirm window has started so a re-asserted DTR from the host close
  / next open cannot cancel the touch.
- The earlier service-port "boot token" workaround (magic byte sequence on
  the SERVICE CDC after arming with line coding `134/8/2/2 + DTR+RTS`) is
  removed from both the firmware and `upload.ps1` — the 1200 bps touch is
  now the single primary trigger.

## Current release truth

### What can be claimed

- local compile/build flows work
- first flash from manual bootloader mode works on the live board
- user-mode single-COM recovery after that first flash works
- **hands-free same-port reupload on this clone works (V1 PASS)**
- the V1 fix is firmware-only and gated by `NRF_SYSTEM_USB_UPLOAD_PREFERRED=1`,
  so it applies to every board family that sets that flag (all `promicro*`,
  `nicenano_v2`, `supermini`, `nrfmicro`, `mini`, `xiao`, `devboard_nrf52840`,
  `pitaya_go`, `usb_dongle`).

### What still needs validation

- V2 (dual-CDC reflash on the SERVICE/MI_00 COM with USER CDC enabled)
- V3 (locked OLD SERVICE COM cycle through enabled/disabled CDC)
- Full board matrix beyond the user's ProMicro clone

## Pointer to the V1 fix

- [`platform/USB_1200_TOUCH_V1_FIX.md`](platform/USB_1200_TOUCH_V1_FIX.md)
