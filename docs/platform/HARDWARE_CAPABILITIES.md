# Hardware Capabilities

Date: 2026-05-04

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

- The hardware family can provide more PWM resources than this package currently exposes.
- The current core exposes a single shared PWM timer group with four routed channels.
- Frequency is shared across active PWM outputs.

## BLE truth

- BLE is currently an advertising-only self-hosted facade.
- There is no connection-oriented GATT server, notification path, or OTA DFU stack in the current package.
