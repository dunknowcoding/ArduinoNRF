# Board Support Notes

Date: 2026-06-09

## Evidence vocabulary

- `verified`: checked on real hardware.
- `modeled`: derived from current variants, package declarations, smoke tests, and source review.
- `reference-core`: derived from comparison against `pdcook/nRFMicro-Arduino-Core`.
- `partial`: coherent package behavior exists, but the full real-board evidence chain is incomplete.

## ProMicro-like boards

Boards:

- `promicro_nrf52840`
- `nicenano_v2`
- `supermini_nrf52840`
- `nrfmicro_nrf52840`

Current truth:

- `promicro_nrf52840` is now verified end-to-end on physical hardware for hands-free UF2 upload, explicit Adafruit serial DFU, UF2 drive-only mode, multi-board UF2 disambiguation, and single-cable USB-CDC debug.
- The verified ProMicro clone can expose the same `NICENANO` UF2 label as a second connected board, so host tooling must disambiguate by stable USB identity and current port role rather than VID/PID or volume label alone.
- `promicro_nrf52840`, `nice!nano v2`, `SuperMini nRF52840`, and `nRFMicro nRF52840` all declare `lfxo` as the low-frequency clock source in this repository.
- `nice!nano v2`, `SuperMini nRF52840`, and `nRFMicro nRF52840` expose secondary-bus pins in metadata and variants.
- The core now exposes global `Wire1` and `SPI1` objects. `promicro_nrf52840` leaves the secondary-bus pins unassigned, while the other three ProMicro-like variants map them to concrete pins.
- `nice!nano v2`, `SuperMini nRF52840`, and `nRFMicro nRF52840` still rely on modeled / reference-core evidence for upload and debug behavior in this revision even though they share the same UF2 / Adafruit serial-DFU family.

Known risks:

- Some SuperMini-class clones may expose a VCC behavior that is unsafe to treat as always-regulated 3.3V when USB is present.
- Battery measurement differs by board: some use `VDDHDIV5`, others use a dedicated VBAT ADC pin.
- Non-ProMicro upload/debug claims in this family are still modeled as the same UF2/DFU + optional double-reset fallback, not re-verified on physical hardware in this revision.

## Remaining families

- `devboard`: generic modeling targets rather than brand-accurate commercial products.
- `xiao-like`: native USB and QSPI modeled, but no battery-sense path.
- `handheld`: battery, QSPI, WiFi coprocessor, and IMU modeled for `pitaya_go_nrf52840`.
- `usb-dongle`: native USB plus SWD pads-only debug modeled.
