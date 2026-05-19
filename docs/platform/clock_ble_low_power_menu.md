# Clock, BLE, and Low-Power Truth

Date: 2026-05-04

## Current declared LFCLK states

- `nicenano_v2`: `lfxo`, evidence `reference-core`
- `supermini_nrf52840`: `lfxo`, evidence `reference-core`
- `nrfmicro_nrf52840`: `lfxo`, evidence `reference-core`
- all remaining packaged boards: `undeclared`

## Consequences

- On boards that remain `undeclared`, `BLE.begin()` fails by design.
- On boards with declared LFCLK metadata, the package only enables the current advertising-only BLE facade.
