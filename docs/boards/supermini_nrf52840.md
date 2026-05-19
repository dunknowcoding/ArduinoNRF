# SuperMini nRF52840

## Current evidence level

- Pin map: partial
- Battery model: partial
- Upload profile: partial
- LFCLK: `lfxo` with `reference-core` evidence

## Current package model

- Family: `promicro-compatible`
- Battery reads are modeled through `VDDHDIV5`.
- `EXT_VCC` control is modeled.
- Secondary-bus pins are modeled as `Wire1 SDA=D13`, `Wire1 SCL=D14`, `SPI1 SCK=D18`, `SPI1 MISO=D19`, `SPI1 MOSI=D20`.

## Caveats

- Some clone boards may expose unsafe or ambiguous VCC behavior when USB is attached.
- The core now exposes global `Wire1` and `SPI1` objects; this board document is only describing the variant-level pin assignment.
