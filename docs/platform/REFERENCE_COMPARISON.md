# Reference Comparison

Date: 2026-05-04

Reference: `pdcook/nRFMicro-Arduino-Core`

## What this repository got wrong or left incomplete

- It originally documented several board files as complete even though they were empty.
- It kept ProMicro-like boards on undeclared LFCLK metadata even where the reference core clearly assumes `USE_LFXO`.
- It modeled `Wire1` and `SPI1` as absent on ProMicro-like boards even where the local pin numbering and the reference core line up well enough to expose those secondary-bus pins.
- It still presents BLE only as a minimal facade and cannot match the reference core's connection-oriented stack.
- It still uses `dfuutil` / `openocd` rather than the reference core's `nrfutil` / bootburn approach.

## What the reference core has that this repository still lacks

- SoftDevice menu integration.
- `adafruit-nrfutil` plus bootburn upload workflows.
- Bluefruit/TinyUSB/nrfx-based ecosystem depth.
- Mature pinout documentation and board images.

## What this repository now does better

- It has explicit truth surfaces such as `NrfBoardInfo`, `NrfBoardSupportStatus`, `NrfBoardPowerInfo`, and `NrfSystemProfile`.
- It has validation sketches aimed at validating declared package truth across multiple board families.
- It has release validation helpers for package-index structure and memory-layout consistency.
- It now exposes real global `Wire1` and `SPI1` objects instead of leaving secondary-bus support at metadata-only truth.

## Current conclusion

This package is stronger than before at telling the truth about its own limits, but it is still not fully adapted to the full ProMicro-like nRF52840 board family standard set by the reference core.
