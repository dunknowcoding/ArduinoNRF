# Hardware Capabilities

Date: 2026-05-04 (PWM/peripheral sections updated 2026-05-29)

> For the authoritative per-peripheral driver census see
> [PERIPHERAL_DRIVER_COVERAGE.md](PERIPHERAL_DRIVER_COVERAGE.md). Every
> discrete nRF52840 peripheral now has a bottom-level driver.

## Chip-level truth currently exposed by the core

- SAADC is present.
- DAC is not present.
- PWM is present.
- The package reports five 32-bit timer/counter blocks at the chip level, while the Arduino-facing core does not currently expose `tone()` or `Servo` APIs.

## ADC truth

- `nrfAdcPresent()` returns `true`.
- `nrfAdcChannelCount()` returns `8`.
- `nrfAdcNativeResolutionBits()` returns `14`.
- The analog-capable raw pins currently modeled by the core are `P0.02`, `P0.03`, `P0.04`, `P0.05`, `P0.28`, `P0.29`, `P0.30`, and `P0.31`.

## PWM truth

- As of v0.1.0 the core exposes all four PWM modules (16 channels total) with
  independent frequency groups, per-pin polarity, and software dead-time
  complementary pairing. See [PWM_MULTI_MODULE.md](PWM_MULTI_MODULE.md).
- The 2026-05-04 note below ("single shared group with four routed channels")
  described the 0.0.1 baseline and no longer holds.

## BLE truth

- Connection-oriented BLE is provided by the vendored **NimBLE** stack
  (`libraries/NimBLE/`): advertising, connections, MTU exchange, and full GATT
  (services / characteristics / descriptors / notifications) are verified on
  hardware against Windows, Android, and board-to-board.
- The legacy self-hosted `BLE` facade remains advertising-only and is superseded
  by NimBLE for anything beyond advertising.
- No SoftDevice and no OTA-DFU-over-BLE stack are shipped.
