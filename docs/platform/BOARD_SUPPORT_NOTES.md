# Board Support Notes

Date: 2026-05-04

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

- These are the boards with the biggest gap between the current package and the reference core.
- `promicro_nrf52840`, `nice!nano v2`, `SuperMini nRF52840`, and `nRFMicro nRF52840` now declare `lfxo` as the low-frequency clock source in this repository.
- `promicro_nrf52840` remains package-modeled for most board facts, but its current physical debug access is now aligned to SWD test pads rather than a populated SWD header.
- `nice!nano v2`, `SuperMini nRF52840`, and `nRFMicro nRF52840` expose secondary-bus pins in metadata and variants.
- The core now exposes global `Wire1` and `SPI1` objects. `promicro_nrf52840` simply leaves the secondary-bus pins unassigned, while the other three ProMicro-like variants map them to concrete pins.

Known risks:

- Some SuperMini-class clones may expose a VCC behavior that is unsafe to treat as always-regulated 3.3V when USB is present.
- Battery measurement differs by board: some use `VDDHDIV5`, others use a dedicated VBAT ADC pin.
- Upload behavior is still modeled as DFU plus optional double-reset fallback, not fully hardware-verified.
- The currently tested `promicro_nrf52840`-class board on `COM3` did not enumerate the expected Nordic DFU VID/PID after `1200bps touch`, so hands-free USB upload cannot yet be claimed for that clone.

## Remaining families

- `devboard`: generic modeling targets rather than brand-accurate commercial products.
- `xiao-like`: native USB and QSPI modeled, but no battery-sense path.
- `handheld`: battery, QSPI, WiFi coprocessor, and IMU modeled for `pitaya_go_nrf52840`.
- `usb-dongle`: native USB plus SWD pads-only debug modeled.
