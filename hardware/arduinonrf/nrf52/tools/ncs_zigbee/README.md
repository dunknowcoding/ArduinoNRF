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
  outside the selected bootloader layout's protected application window.
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

For board1-board3 no-SoftDevice ProMicro/nice!nano-style bootloaders, build the
bootloader-preserving NCP USB image instead:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  hardware\arduinonrf\nrf52\tools\ncs_zigbee\build_zigbee.ps1 `
  -Board promicro_nrf52840 -Target ncp_usb -ImageLayout no-softdevice `
  -Pristine always
```

This uses a short build path under `.ncs-zigbee-work\b\an\pm40\ncp-nosd`,
disables MCUboot/sysbuild for this layout, and verifies the static partitions
after the build:

```text
0x0000..0x0FFF  low bootloader / MBR guard
0x1000..0xDFFFF application
0xE0000..0xE7FFF ZBOSS NVRAM
0xE8000..0xE8FFF ZBOSS product config
0xE9000..0xFFFFF top UF2 bootloader guard
```

Dry-run the flash guard before touching hardware:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  hardware\arduinonrf\nrf52\tools\ncs_zigbee\flash_zigbee.ps1 `
  -Board promicro_nrf52840 -BootloaderLayout no-softdevice `
  -Hex .ncs-zigbee-work\b\an\pm40\ncp-nosd\zephyr\zephyr.hex -DryRun
```

board1 was flashed successfully with this image over J-Link. Post-flash
readback confirmed the bootloader vectors at `0x00000000` were unchanged, and
Windows enumerated the running Zephyr NCP USB firmware as `VID_2FE3&PID_0001`
on `COM27`.
