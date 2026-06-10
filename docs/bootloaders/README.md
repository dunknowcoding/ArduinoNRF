# Bootloader Notes

Date: 2026-06-09

## Bootloader families in this package

### UF2 mass storage

UF2 is now a first-class Windows upload path for boards that expose a UF2 drive. `upload.ps1` converts the sketch HEX to UF2, matches the drive to the selected upload COM by stable USB identity, copies the file, and resets the board.

The **Upload Method -> Enter UF2 drive only (no upload)** menu is for inspection/recovery: it enters bootloader mode, reports the matched drive, and stops before copying firmware.

### Adafruit serial DFU

Adafruit serial DFU remains available through explicit bootloader menu entries such as `Serial DFU, SoftDevice BLE`. On Windows the package uses `upload.ps1`; on Linux/macOS it uses `upload.py` plus `adafruit-nrfutil`.

Boards packaged on this family include the validated AliExpress ProMicro nRF52840 path plus the modeled nice!nano v2, SuperMini, nRFMicro, XIAO, and Pitaya Go definitions. See [../uploads/hands_free_upload.md](../uploads/hands_free_upload.md) and [../COMPATIBILITY.md](../COMPATIBILITY.md).

### Nordic Open DFU

The `usb_dongle_nrf52840` target (PCA10059) uses Nordic Open DFU, not Adafruit serial DFU. Use Nordic tooling such as `nrfutil` or nRF Connect for Desktop rather than the package's default `niusdfu` flow for that board.

### SWD / J-Link / OpenOCD

Generic dev boards, official Nordic DK-style hardware, and any board exposing usable pads can be flashed over SWD.

For normal sketch upload, choose the probe directly from **Tools -> Upload
Method**:

- `SWD programmer (CMSIS-DAP)`
- `SWD programmer (SEGGER J-Link)`

For **Sketch -> Upload Using Programmer** and **Tools -> Burn Bootloader**,
choose the probe from **Tools -> Programmer** instead.

On Windows, the J-Link paths use SEGGER's command-line tools. This avoids the
common OpenOCD/libusb failure mode on machines that have the official SEGGER
driver installed. CMSIS-DAP paths continue to use OpenOCD.

Arduino IDE 2 SWD Debug follows the selected **Upload Method**: CMSIS-DAP keeps
the OpenOCD server, while the J-Link upload method switches the debug metadata
to Arduino IDE's `jlink` server type.

Arduino IDE **Tools -> Burn Bootloader** is available for the nice!nano-family
definitions that have a known bundled bootloader image:

- `promicro_nrf52840`
- `nicenano_v2`
- `supermini_nrf52840`

Select **Tools -> Programmer -> SEGGER J-Link (SWD)** or **CMSIS-DAP (SWD)**
first. The wrapper performs a chip erase/recover and then programs the bundled
nice!nano bootloader HEX. J-Link uses SEGGER `JLink.exe`; CMSIS-DAP uses
OpenOCD `nrf52_recover`.

`hardware/arduinonrf/nrf52/bootloaders/nice_nano/nice_nano_bootloader-0.6.0_s140_6.1.1.hex`

After burning this image, select a S140 v6 / `0x26000` Bootloader / DFU layout
before compiling sketches. Do not use the no-SoftDevice `0x1000` menu entries
with this bootloader image.

Burn Bootloader is intentionally **not** exposed as a single-USB operation. If
the existing bootloader still works, USB DFU bootloader updates can be done with
the vendor DFU package, but IDE Burn Bootloader is the recovery path and requires
SWD.

## Current package assumption

The package assumes either USB DFU or SWD-based upload depending on the board
definition. Bootloader replacement is limited to boards with an explicitly
configured, bundled bootloader image and is always SWD-only.
