# ArduinoNRF nCS Zigbee R23 Sidecar

This directory is the tracked, lightweight entry point for Nordic's official
Zigbee R23 path. It does not vendor the nRF Connect SDK, the Zigbee add-on, or
ZBOSS binaries into ArduinoNRF.

Local downloads and build outputs belong in the repository root:

```text
.ncs-zigbee-work/
```

That directory is ignored by Git. Delete it whenever you want a clean workspace.

## Current first target

- Board: `promicro_nrf52840`, physical `board1`
- Programmer: SEGGER J-Link over SWD
- Bootloader policy: never reflash bootloader by default
- Firmware path: nCS Zigbee R23 add-on sidecar

## Tools

- `build_zigbee.ps1`: checks the environment and builds Nordic official
  `ncs-zigbee` samples through `west build`. On Windows it auto-detects the
  `IronEngineWorld` conda environment when present and the latest
  `C:\ncs\toolchains\*\environment.json` toolchain install.
- `flash_zigbee.ps1`: flashes an already-built sidecar `.hex` through J-Link.
  It refuses bootloader/recover style actions and rejects HEX files that write
  below the selected bootloader layout's `app_start`.
- `pins.json`: records the intended official stack versions and local policy.

The current build target is a reference build against Nordic's supported Zephyr
boards, for example mapping `promicro_nrf52840` to `nrf52840dk/nrf52840` for the
first `ncp_usb` stack smoke test. Do not flash `merged.hex` from this reference
build to Arduino bootloader boards; it contains a full sysbuild image starting
at address `0x0`.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  hardware\arduinonrf\nrf52\tools\ncs_zigbee\build_zigbee.ps1 `
  -Board promicro_nrf52840 -Target ncp_usb -Pristine always
```
