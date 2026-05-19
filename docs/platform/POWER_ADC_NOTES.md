# Power and ADC Notes

Date: 2026-05-04

## ADC and power-path distinction

The current package distinguishes between three different concepts:

- normal external analog pins,
- a dedicated board VBAT pin that consumes one ADC-capable input,
- the nRF52 `VDDHDIV5` internal measurement path.

## Board-level battery models

- `promicro_nrf52840`: battery sense is modeled on public pin `29`.
- `nicenano_v2`: battery sense is modeled through `VDDHDIV5`.
- `supermini_nrf52840`: battery sense is modeled through `VDDHDIV5`.
- `nrfmicro_nrf52840`: battery sense is modeled on `VBAT`.
- `mini_nrf52840`: battery sense is modeled on public pin `31`.
- `pitaya_go_nrf52840`: battery sense is modeled on public pin `28`.

## Practical caveats

- When a board uses a public VBAT pin, that pin is no longer a free generic analog input in practical board usage.
- When a board uses `VDDHDIV5`, there is no dedicated public battery pin, but battery reads come from the internal divided supply path.
