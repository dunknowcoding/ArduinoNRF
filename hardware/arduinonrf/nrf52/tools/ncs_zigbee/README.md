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

- `build_zigbee.ps1`: checks the environment and, when an nCS workspace exists,
  delegates to `west build`.
- `flash_zigbee.ps1`: flashes an already-built sidecar `.hex` through J-Link.
  It refuses bootloader/recover style actions.
- `pins.json`: records the intended official stack versions and local policy.

The first implementation phase is intentionally conservative: build and flash
official sidecar firmware, then document the observed hardware behavior.
