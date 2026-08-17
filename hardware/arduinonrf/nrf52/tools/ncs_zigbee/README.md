# ArduinoNRF nCS Zigbee R23 Sidecar

This directory is the tracked, lightweight entry point for Nordic's official
Zigbee R23 path. It does not vendor the nRF Connect SDK, the Zigbee add-on, or
ZBOSS binaries into ArduinoNRF.

Local downloads and build outputs belong in the repository root:

```text
.ncs-zigbee-work/
```

That directory is ignored by Git. Delete it whenever you want a clean workspace.

## Reference target

- Board: `promicro_nrf52840`
- Programmer: supported SWD probe
- Bootloader policy: never reflash bootloader by default
- Firmware path: nCS Zigbee R23 add-on sidecar

## Tools

- `build_zigbee.ps1`: checks the environment and builds Nordic official
  `ncs-zigbee` samples through `west build`. On Windows it uses the active
  Conda environment when suitable, `NCS_TOOLCHAIN_ROOT`, or a standard nCS
  toolchain install.
- `flash_zigbee.ps1`: flashes an already-built sidecar `.hex` through J-Link.
  It refuses bootloader/recover style actions and rejects HEX files that write
  outside the selected bootloader layout's protected application window.
- `ncp_host.ps1`: prepares Nordic's official ZBOSS NCP Host package and checks
  the Windows-to-WSL path for running the Linux `simple_gw` host application.
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

For no-SoftDevice ProMicro/nice!nano-style bootloaders, build the
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

The reference hardware was flashed successfully with this image over SWD. Post-flash
readback confirmed the bootloader vectors at `0x00000000` were unchanged, and
Windows enumerated the running Zephyr NCP USB firmware as `VID_2FE3&PID_0001`.

## Official NCP Host package

The Nordic NCP Host side is distributed as a Linux package. Prepare it under the
ignored workspace with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  hardware\arduinonrf\nrf52\tools\ncs_zigbee\ncp_host.ps1 `
  -Port $env:NCP_PORT -Download -Extract
```

The package is downloaded from the `ncs-zigbee v1.3.0` release as
`ncp_host_v3.6.0.zip`. The included `simple_gw` binary is a 64-bit Linux ELF and
requires Ubuntu/WSL or a Linux host.

On Windows, first check the host status:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  hardware\arduinonrf\nrf52\tools\ncs_zigbee\ncp_host.ps1 `
  -Port $env:NCP_PORT
```

If it reports `Windows optional component: missing or pending reboot`, run this
once from an elevated PowerShell and reboot Windows:

```powershell
wsl --install --no-distribution
```

After reboot, install Ubuntu 22.04 for this workflow:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  hardware\arduinonrf\nrf52\tools\ncs_zigbee\ncp_host.ps1 `
  -Port $env:NCP_PORT -InstallUbuntu -WslDistro Ubuntu
```

On WSL2, Windows COM ports are not automatically exposed as `/dev/ttyS*`.
Install `usbipd-win`, find the board's BUSID, and attach the Zephyr NCP USB
device to Ubuntu. Obtain the device BUSID from `usbipd list` rather than
hard-coding a host-specific value:

```powershell
winget install --id dorssel.usbipd-win --source winget

powershell -NoProfile -ExecutionPolicy Bypass -File `
  hardware\arduinonrf\nrf52\tools\ncs_zigbee\ncp_host.ps1 `
  -Port $env:NCP_PORT -UsbBusId $env:NCP_USB_BUS_ID -WslDistro Ubuntu `
  -WslTty /dev/ttyACM0 -AttachUsb
```

`usbipd bind` requires an elevated PowerShell. After USB pass-through is active,
the wrapper can start the official gateway against the Linux CDC ACM device.
The WSL2 USB/IP path can be fragile with this Zephyr CDC device; if it attaches
and then disconnects with `connection reset by peer`, use the WSL1 fallback
below.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  hardware\arduinonrf\nrf52\tools\ncs_zigbee\ncp_host.ps1 `
  -Port $env:NCP_PORT -WslTty /dev/ttyACM0 -RunSimpleGw
```

WSL1 fallback for Windows COM ports:

```powershell
wsl --install Ubuntu-22.04 --name ArduinoNRF-Ubuntu1 `
  --version 1 --no-launch --web-download

powershell -NoProfile -ExecutionPolicy Bypass -File `
  hardware\arduinonrf\nrf52\tools\ncs_zigbee\ncp_host.ps1 `
  -Port $env:NCP_PORT -WslDistro ArduinoNRF-Ubuntu1 -WslTty $env:NCP_WSL_TTY `
  -RunSimpleGw
```

For bounded validation runs, add `-RunSeconds <seconds>`. The helper uses the
official host invocation form:

```text
NCP_SLAVE_PTY=<linux-serial-port> ./application/simple_gw/simple_gw
```
