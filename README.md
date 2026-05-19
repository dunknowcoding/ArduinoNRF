# Arduino NRF52 Package

This repository contains a local Arduino platform package for nRF52 boards under `hardware/arduinonrf/nrf52`.

## Current scope

- Truth-oriented board metadata for the packaged nRF52 variants.
- Local Board Manager archive generation and package index validation.
- Compile smoke coverage for hardware capability, battery sense, upload policy, BLE facade, PWM behavior, bus parameter changes, and secondary-bus pin truth.

## Packaged boards

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

## Current hardware-truth summary

- ADC support exists in the local core and exposes SAADC-backed reads plus `analogReadVDD()` and `analogReadVDDHDIV5()`.
- PWM is currently a single shared `PWM0` group with up to four simultaneously routed outputs and one shared frequency domain.
- BLE is currently an advertising-only self-hosted facade, not a full connection-oriented GATT stack.
- `nice!nano v2`, `SuperMini nRF52840`, and `nRFMicro nRF52840` now declare reference-backed LFCLK metadata and secondary-bus pins in package metadata.
- The core now exposes global `Wire1` and `SPI1` objects; boards without verified secondary buses leave those pins unassigned and refuse to enable the extra bus at runtime.
- On the user's ProMicro clone, **manual-boot first flash works, the board returns to user mode, and `promicroserialnosd + usbcdc=disabled` now supports same-COM reupload on the same USB cable**.

## Gaps versus mature ProMicro-like cores

- No bundled `adafruit-nrfutil` / bootburn upload path.
- No SoftDevice menu or Bluefruit/TinyUSB-based BLE stack.
- No broad claim yet that every clone-family USB profile is interchangeable; the currently validated real-board path is `promicroserialnosd`, with:
  - `usbcdc=disabled`: same-COM reupload works on the single visible maintenance COM.
  - `usbcdc=enabled`: uploads must use the service/maintenance CDC (`MI_00`); selecting the user CDC (`MI_02`) is rejected with a clear error.
- No source-level USB-only debug path without an external probe; current IDE debug integration still relies on SWD/OpenOCD.
- Some SuperMini-class clone boards may expose VCC behavior that should not be treated as universally safe 3.3V when USB power is present.

## Documentation

All project documentation other than this root README lives under `docs/`.

- `docs/release/README.md`: local release flow.
- `docs/release/PRE_RELEASE_CHECKLIST.md`: pre-release validation checklist.
- `docs/platform/REFERENCE_COMPARISON.md`: comparison against `pdcook/nRFMicro-Arduino-Core`.
- `docs/platform/BOARD_SUPPORT_STATUS.md`: current board support matrix.
- `docs/platform/BOARD_SUPPORT_NOTES.md`: board-family notes and caveats.
- `docs/boards/README.md`: overview of per-board documents.
- `docs/CURRENT_ACCEPTANCE_REPORT.md`: current real-board progress snapshot.
