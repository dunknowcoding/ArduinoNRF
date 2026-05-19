# nRFMicro nRF52840

## Current evidence level

- Pin map: partial
- Battery model: partial to strong package model
- Upload profile: partial
- LFCLK: `lfxo` with `reference-core` evidence

## Current package model

- Family: `promicro-compatible`
- Battery is modeled on dedicated `VBAT`.
- `EXT_VCC` control is modeled.
- Secondary-bus pins are modeled as `Wire1 SDA=D22`, `Wire1 SCL=D23`, `SPI1 SCK=D18`, `SPI1 MISO=D19`, `SPI1 MOSI=D20`.

## Caveats

- The core now exposes global `Wire1` and `SPI1` objects; this board document is only describing the variant-level pin assignment.
