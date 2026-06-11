# Power and ADC Notes

Last revised: 2026-06-10 (pin models re-checked against the variants).

## ADC and power-path distinction

The package distinguishes three concepts:

- normal external analog pins,
- a dedicated board VBAT pin that consumes one ADC-capable input,
- the nRF52 `VDDHDIV5` internal measurement path (the divided high-voltage
  supply, no external pin involved).

## Reading them from a sketch

```cpp
int raw  = analogRead(A0);           // any external analog pin
int vdd  = analogReadVDD();          // the MCU supply itself
int div5 = analogReadVDDHDIV5();     // VDDH / 5 internal path

bool ok      = nrfBatteryReadingSupported();
int  batt    = nrfBatteryRaw();         // routes to the board's model below
uint32_t mv  = nrfBatteryMillivolts();  // same reading, converted
```

`NrfBoardPowerInfo` (see `NrfBoard.h`) reports which model a board uses at
runtime: `batterySenseAvailable`, `batterySenseViaVddhDiv5`, and
`batterySensePin` (`0xFF` when the board has no dedicated pin).

## Board-level battery models

| Board | Model |
| --- | --- |
| `promicro_nrf52840` | VBAT divider on public pin `A1` / D16 (P0.29) |
| `nicenano_v2` | `VDDHDIV5` (no public battery pin) |
| `supermini_nrf52840` | `VDDHDIV5` (no public battery pin) |
| `nrfmicro_nrf52840` | dedicated `VBAT` pin (P0.04) |
| `mini_nrf52840` | public pin `31` |
| `pitaya_go_nrf52840` | public pin `28` |
| `xiao_nrf52840`, `devboard_*`, `usb_dongle_nrf52840` | no battery sense modeled (`PIN_BATTERY = 255`) |

## Practical caveats

- When a board uses a public VBAT pin, that pin is no longer a free generic
  analog input in practical board usage.
- When a board uses `VDDHDIV5`, there is no dedicated public battery pin;
  battery reads come from the internal divided supply path, which only makes
  sense when the battery actually feeds VDDH.
- Battery percentage interpretation (divider ratios, chemistry curves) is
  out of scope for the core — it returns the raw ADC reading of the modeled
  path.
