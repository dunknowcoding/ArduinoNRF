# AliExpress ProMicro nRF52840

## Current evidence level

- Pin map: partial; **primary I2C pins hardware-verified 2026-06-03** (see below)
- Battery model: partial
- Upload profile: verified on hardware for `bootloader=promicroserialnosd,usbcdc=enabled`
- LFCLK: `lfxo` with schematic evidence
- Debug access: user-image-backed SWD pads
- Single-cable USB-CDC GDB stub: verified on hardware

## Current upload baseline

| Item | Current truth |
| ------ | --------------- |
| Active clone menu under test | `bootloader=promicroserialnosd` |
| Application start | `0x1000` |
| Bootloader family | `0x239A:0x00B3` |
| Upload transport | Adafruit serial DFU through `upload.ps1` |
| Runtime service COM in `promicroserialnosd` | may still appear under the same `0x239A:0x00B3` family |
| Software reset target in code | full bootloader (`GPREGRET = 0x57`) |
| Current status | V1 hands-free reflash and USB-CDC GDB-stub debug are both PASSING; see [`../VALIDATION.md`](../VALIDATION.md) |

## Current live-board status

### Verified

- manual bootloader entry on `COM3` can flash the board
- after the first flash, the board returns to user mode
- with `usbcdc=enabled`, the board exposes separate user and service CDC paths
- selecting the user CDC for upload is rejected; the service CDC hands-free reupload path works repeatedly without manual reset
- with `usbcdc=disabled`, hands-free in-app upload also works on the single service CDC (verified 3× back-to-back and across `usbcdc` transitions both ways)
- USB-only GDB-stub debug over the service CDC works with breakpoints, step, registers, memory, watchpoints, and pause

### Pinout — Arduino number == silk-screen "Dn"

`digitalWrite(6, ...)` drives the pad marked **D6**, `Wire.begin()` uses the
**SDA**/**SCL** pads, `analogRead(A0)` reads the **A0** pad, etc.

| Dn | nRF | Silk | Function | Dn | nRF | Silk | Function |
| -- | --- | ---- | -------- | -- | --- | ---- | -------- |
| D0 | P0.06 | TX | UART TX | D11 | P0.10 | NFC2 | GPIO (NFC) |
| D1 | P0.08 | RX | UART RX | D12 | P1.11 | | GPIO |
| D2 | P0.17 | SCK | SPI SCK | D13 | P1.13 | SDA1 | `Wire1` SDA |
| D3 | P0.20 | MISO | SPI MISO | D14 | P1.15 | SCL1 | `Wire1` SCL |
| D4 | P0.22 | MOSI | SPI MOSI | D15 | P0.02 | A0 | `A0` / AIN0 |
| D5 | P0.24 | CS | SPI SS | D16 | P0.29 | A1 | `A1` / AIN5 |
| **D6** | **P1.00** | **SDA** | **`Wire` SDA** | D17 | P0.31 | A2 | `A2` / AIN7 |
| **D7** | **P0.11** | **SCL** | **`Wire` SCL** | D18 | P1.01 | SCK1 | `SPI1` SCK |
| D8 | P1.04 | | GPIO | D19 | P1.02 | MISO1 | `SPI1` MISO |
| D9 | P1.06 | | GPIO | D20 | P1.07 | MOSI1 | `SPI1` MOSI |
| D10 | P0.09 | NFC1 | GPIO (NFC) | — | P0.15 | — | `LED_BUILTIN` (orange, active-high) |

Convenience constants: `SDA`/`SCL` (D6/D7), `SCK`/`MISO`/`MOSI`/`SS` (D2–D5),
`A0`–`A2` (D15–D17), `SDA1`/`SCL1` (D13/D14), `SCK1`/`MISO1`/`MOSI1` (D18–D20),
`LED_BUILTIN` (P0.15). `Wire.begin(SDA, SCL)` / `SPI.begin(SCK, MISO, MOSI)`
also accept explicit pin numbers to remap a bus at runtime.

The I2C SDA/SCL routing (D6→P1.00, D7→P0.11) and the LED (P0.15) are
hardware-verified via J-Link SWD; D10/D11 (P0.09/P0.10) are the NFC antenna
pins and become plain GPIO automatically the first time you drive them (takes
effect after the next reset). The remaining D-pads follow the board
silk-screen.

> Note: I2C *routing* is verified, but a working I2C *sensor* still depends on
> your wiring — confirm GND and the sensor's nCS/CS (an MPU-9250 module with CS
> low boots into SPI mode and never answers on I2C).

### Still board-specific

- For `promicroserialnosd`, runtime and bootloader phase detection cannot rely on VID/PID alone.

## Current package model

- Family: `promicro-compatible`
- Battery sense is modeled on P0.29 (D16 / A1).
- `EXT_VCC` control modeled on P0.13.
- SWD is modeled as exposed test pads rather than a populated header.

## Notes

- The current source-of-truth docs are:
  - `docs/VALIDATION.md`
  - `docs/platform/UPLOAD_BEHAVIOR.md`
