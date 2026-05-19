# Release Notes — v0.1.2

Cosmetic-only iteration on top of [v0.1.1](RELEASE_NOTES_v0.1.1.md). Same
install URL:

```
https://raw.githubusercontent.com/dunknowcoding/ArduinoNRF/main/package_arduinonrf_index.json
```

If you already installed `0.1.0` or `0.1.1`, open Arduino IDE 2.x's
**Tools → Board → Boards Manager**, search for **ArduinoNRF**, and click
**Update**.

## What changed

- **Display name**: `Arduino NRF52 Boards` → `ArduinoNRF`. The previous
  name was too generic and could be confused with Arduino-official nRF52
  platforms. The package ID (`arduinonrf`), architecture (`nrf52`), and
  the FQBN prefix (`arduinonrf:nrf52:...`) are unchanged — no breaking
  change for sketches, scripts, or CI configs that hard-code the FQBN.

Everything else from `0.1.1` carries forward unchanged (one-click Debug
via the openocd-impersonating bridge wrapper, V1/V2/V3 button-less
reflash, the firmware-side EP0 OUT / latch fixes).
