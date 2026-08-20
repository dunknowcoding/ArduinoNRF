# AliExpress ProMicro nRF52840

## Current evidence level

- Pin map: partial; **primary I2C pins hardware-verified 2026-06-03** (see below)
- Battery model: partial
- Upload profile: verified on hardware for `uploadmode=usb,bootloader=auto`, explicit UF2, and explicit Adafruit serial DFU
- LFCLK: `lfxo` with schematic evidence
- Debug access: user-image-backed SWD pads
- Single-cable USB-CDC GDB stub: verified on hardware

## Current upload baseline

| Item | Current truth |
| ------ | --------------- |
| Active clone menu under test | `uploadmode=usb,bootloader=auto` |
| Application start | `0x26000` on SoftDevice S140 v6 UF2 builds; `0x1000` when the bootloader reports `SoftDevice: not found`; `0x27000` for S140 v7/legacy layouts |
| Bootloader family | `0x239A:0x00B3` |
| Upload transport | UF2 mass storage or Adafruit serial DFU through `upload.ps1` |
| Runtime service COM | may re-enumerate after upload; always select the current service/DFU COM |
| Software reset target in code | full bootloader (`GPREGRET = 0x57`) |
| Current status | UF2 upload, explicit serial DFU, UF2-drive-only, multi-board matching, hands-free reflash, and USB-CDC GDB-stub debug are supported; see [`../platform/UPLOAD_BEHAVIOR.md`](../platform/UPLOAD_BEHAVIOR.md) |

## Current live-board status

### Verified

- manual bootloader entry on `COM3` can flash the board
- after the first flash, the board returns to user mode
- with `usbcdc=enabled`, the board exposes separate user and service CDC paths
- selecting either CDC for upload resolves to the same device's service interface; hands-free reupload works repeatedly without manual reset
- UF2 upload from DFU mode works with both `bootloader=auto` and explicit UF2 menu selection
- explicit Adafruit serial DFU from DFU mode works while another UF2 drive is mounted
- `Upload Method -> Enter UF2 drive only (no upload)` mounts the selected board and stops before copying firmware
- stale COM selection is rejected so a second mounted `NICENANO` drive is not used accidentally
- with `usbcdc=disabled`, hands-free in-app upload also works on the single service CDC (verified 3× back-to-back and across `usbcdc` transitions both ways)
- USB-only GDB-stub debug over the service CDC works with breakpoints, step, registers, memory, watchpoints, and pause
- no-SoftDevice nice!nano-compatible bootloaders (`INFO_UF2.TXT`: `SoftDevice: not found`) run sketches correctly with the no-SoftDevice menu entries (`bootloader=autonosd`, `bootloader=promicronosduf2`, or `bootloader=promicroserialnosd`)
- SEGGER J-Link SWD application upload works from `Tools -> Upload Method -> SWD programmer (SEGGER J-Link)`; `Sketch -> Upload Using Programmer` with `Tools -> Programmer -> SEGGER J-Link (SWD)` also works
- the package keeps explicit SWD upload modes aligned with the current board definitions so application-only SWD upload does not require `Burn Bootloader`
- CC2530 external radio workflow is verified: D8/D9/D10 debug flash, D0/D1 runtime UART, and two-node `CC2530_Link` traffic on channel 11

### Bootloader layout choices

The ProMicro clone can identify as `NICENANO` / `nice!nano` in both the normal
S140 v6 layout and a bootloader/MBR-only layout. The name is therefore not
enough to choose the linker origin:

| UF2 `INFO_UF2.TXT` / bootloader state | Arduino IDE `Bootloader / DFU` choice |
|---|---|
| Original nice!nano v2-style SoftDevice S140 v6 | `Auto-detect upload, SoftDevice S140 v6 layout (0x26000)` or `UF2 mass storage, SoftDevice S140 v6 layout (0x26000)` |
| `SoftDevice: not found` | `Auto-detect upload, no SoftDevice / MBR only (0x1000)`, `UF2 mass storage, no SoftDevice / MBR only (0x1000)`, or `Serial DFU, no SoftDevice / MBR only (0x1000)` |
| S140 v7 / legacy | `UF2 mass storage, SoftDevice S140 v7 / legacy layout (0x27000)` or `Serial DFU, SoftDevice S140 v7 / legacy layout (0x27000)` |

`upload.ps1` refuses UF2 uploads when the mounted bootloader reports an app
start that does not match the IDE option used for compilation. This catches the
common failure mode where upload appears successful but the app never starts.

### Burn Bootloader

Arduino IDE `Tools -> Burn Bootloader` is wired for this board through SWD only.
Use `Tools -> Programmer -> SEGGER J-Link (SWD)` or `CMSIS-DAP (SWD)`, then run
Burn Bootloader. The recipe performs the recover/erase step first and only then
flashes the bundled ProMicro bootloader image
`promicro_nrf52840_bootloader-0.9.2_s140_6.1.1.hex`.

On Windows, the J-Link path uses SEGGER `JLink.exe`; CMSIS-DAP uses OpenOCD for
the recover step. The recover step is what clears `APPROTECT` on a locked
device; the bootloader HEX is flashed after recover and does not disable
protection by itself. The J-Link upload/programmer paths were hardware-verified
with application-only uploads, and the ProMicro bootloader image was verified
after a full SWD recovery flow.

After this, the board is back on the S140 v6 layout; select a `0x26000`
Bootloader / DFU option before compiling sketches. Do not use this command when
you only want to upload an application.

### SWD sketch upload

For application-only SWD upload, do not use Burn Bootloader. Select
`Tools -> Upload Method -> SWD programmer (SEGGER J-Link)`,
`SWD programmer (CMSIS-DAP)`, then click the normal Upload button.

`Tools -> Programmer` is only needed for `Sketch -> Upload Using Programmer`
and `Tools -> Burn Bootloader`; it does not change the normal Upload Method
recipe.

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

- Runtime and bootloader phase detection cannot rely on VID/PID alone; use the service/DFU COM that Arduino IDE currently shows.

## Current package model

- Family: `promicro-compatible`
- Battery sense is modeled on P0.29 (D16 / A1).
- `EXT_VCC` control modeled on P0.13.
- SWD is modeled as exposed test pads rather than a populated header.

## Notes

- The current source-of-truth docs are:
  - `docs/platform/UPLOAD_BEHAVIOR.md`
  - `docs/platform/UPLOAD_BEHAVIOR.md`
