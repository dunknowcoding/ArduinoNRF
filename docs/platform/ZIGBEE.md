# Zigbee / IEEE 802.15.4 on ArduinoNRF

ArduinoNRF now tracks two Zigbee paths:

1. **Official onboard Zigbee firmware** for the nRF52840's own RADIO, built with
   Nordic's nRF Connect SDK Zigbee R23 add-on. This is the future-facing path.
2. **External CC2530 Zigbee / 802.15.4**, the already verified Arduino path that
   uses a UART radio module and the separate NiusZigbee library.

These paths solve different problems. The onboard path runs official Nordic
firmware on the nRF52840 and replaces the Arduino sketch. The CC2530 path keeps
the Arduino sketch running on the nRF52840 and uses an external transceiver.

## Official onboard Zigbee: nCS Zigbee R23 sidecar

This is the priority path for "nRF52840 without CC2530".

The first implementation target is a sidecar firmware flow:

```text
nRF Connect SDK + Zigbee R23 add-on -> official Zigbee firmware .hex
ArduinoNRF board metadata/tools      -> board overlay, build wrapper, flash wrapper
board1 + J-Link                      -> first hardware validation target
```

Tracked files:

- Tools: [`hardware/arduinonrf/nrf52/tools/ncs_zigbee/`](../../hardware/arduinonrf/nrf52/tools/ncs_zigbee/)
- Pins: [`pins.json`](../../hardware/arduinonrf/nrf52/tools/ncs_zigbee/pins.json)
- Planning note: `F:\Arduino\driver\arduinonrf_improve.md` in the local driver
  workspace.

### What this path means

- Uses the nRF52840's onboard IEEE 802.15.4 RADIO.
- Uses Nordic's official Zigbee R23 add-on / ZBOSS stack through nCS.
- First firmware targets are `ncp_usb`, `shell`, and `coordinator`.
- First board target is physical **board1** (`promicro_nrf52840`) because it has
  a J-Link connected.
- Bootloaders are not reflashed by default.
- Local SDK downloads and builds go in `.ncs-zigbee-work/`, which is ignored by
  Git.

### What this path does not mean yet

- It does not make `Zigbee.begin()` in a normal Arduino sketch run ZBOSS.
- It does not run NimBLE and Zigbee together in the current bare-metal Arduino
  runtime.
- It does not promise Zigbee product certification.
- It does not bundle ZBOSS binaries inside ArduinoNRF.

### Check the sidecar environment

From the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  hardware\arduinonrf\nrf52\tools\ncs_zigbee\build_zigbee.ps1 `
  -Board promicro_nrf52840 -Target ncp_usb -CheckOnly
```

Expected: the script prints the selected board/target, checks for `west`,
`cmake`, and Python, and creates `.ncs-zigbee-work/` if needed. In check-only
mode it does not download, build, or flash anything.

### Build the official reference firmware

With an initialized `ncs-zigbee` workspace and Nordic toolchain:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  hardware\arduinonrf\nrf52\tools\ncs_zigbee\build_zigbee.ps1 `
  -Board promicro_nrf52840 -Target ncp_usb -Pristine always
```

For the first smoke test this maps `promicro_nrf52840` to Nordic's supported
`nrf52840dk/nrf52840` Zephyr board and builds the official Zigbee NCP USB sample
with `FILE_SUFFIX=usb`. This validates the official nRF52840 Zigbee R23 stack
without touching connected boards.

Generated reference artifacts are placed under:

```text
.ncs-zigbee-work/build/arduinonrf/promicro_nrf52840/ncp_usb/
```

The generated `merged.hex` is a full sysbuild image that starts at address
`0x0`. Do not flash it to Arduino bootloader boards. The ProMicro/nice!nano
bootloader-preserving image is built separately with `-ImageLayout no-softdevice`.

### Build a no-SoftDevice bootloader-preserving NCP USB image

For board1-board3 style no-SoftDevice ProMicro/nice!nano bootloaders, build the
application-only NCP USB image with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  hardware\arduinonrf\nrf52\tools\ncs_zigbee\build_zigbee.ps1 `
  -Board promicro_nrf52840 -Target ncp_usb -ImageLayout no-softdevice `
  -Pristine always
```

This layout disables MCUboot/sysbuild for the ProMicro path and uses a static
Partition Manager layout that preserves:

- `0x0000..0x0FFF`: low bootloader/MBR guard
- `0x1000..0xDFFFF`: application
- `0xE0000..0xE7FFF`: ZBOSS NVRAM
- `0xE8000..0xE8FFF`: ZBOSS product config
- `0xE9000..0xFFFFF`: top UF2 bootloader guard

The build wrapper verifies these partitions after `west build`. The flashable
artifact for SWD/J-Link application flashing is:

```text
.ncs-zigbee-work/b/an/pm40/ncp-nosd/zephyr/zephyr.hex
```

### Flash policy

The sidecar flash wrapper is intentionally narrow:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  hardware\arduinonrf\nrf52\tools\ncs_zigbee\flash_zigbee.ps1 `
  -Board promicro_nrf52840 -BootloaderLayout no-softdevice `
  -Hex path\to\bootloader-preserving-sidecar.hex
```

It uses J-Link application flashing only. It does not run `recover`, `eraseall`,
or bootloader flashing commands. By default it reads the HEX address range and
refuses files that write below the selected layout's `app_start` or at/above
the selected layout's protected `app_end`.

For board1 no-SoftDevice dry-run validation:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  hardware\arduinonrf\nrf52\tools\ncs_zigbee\flash_zigbee.ps1 `
  -Board promicro_nrf52840 -BootloaderLayout no-softdevice `
  -Hex .ncs-zigbee-work\b\an\pm40\ncp-nosd\zephyr\zephyr.hex -DryRun
```

board1 hardware validation has also passed with the same HEX and J-Link path:
the bootloader vectors at `0x00000000` were unchanged after flashing, the Zephyr
application vector table was present at `0x00001000`, and Windows enumerated
the device as Zephyr USB CDC `VID_2FE3&PID_0001`, `COM27`.

### NCP host validation path

Nordic's official host side for this firmware is the ZBOSS NCP Host package.
For `ncs-zigbee v1.3.0`, the matching package is `ncp_host_v3.6.0.zip` from the
same GitHub release. ArduinoNRF provides a small helper to download, extract,
and check this package under `.ncs-zigbee-work/`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  hardware\arduinonrf\nrf52\tools\ncs_zigbee\ncp_host.ps1 `
  -Port COM27 -Download -Extract
```

The included `application/simple_gw/simple_gw` executable is a 64-bit Linux ELF.
It is not a native Windows executable. The expected Windows development path is
to run it through Ubuntu/WSL or on a Linux host, with the board's Windows COM
port mapped to the equivalent Linux serial device, for example `COM27` ->
`/dev/ttyS27`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  hardware\arduinonrf\nrf52\tools\ncs_zigbee\ncp_host.ps1 `
  -Port COM27 -RunSimpleGw
```

If the wrapper reports `Ubuntu: not available`, install or enable an Ubuntu WSL
distribution before attempting host protocol validation. On Windows, first run
the status command without `-RunSimpleGw`; if it reports
`Windows optional component: missing or pending reboot`, enable WSL from an
elevated PowerShell and reboot:

```powershell
wsl --install --no-distribution
```

Then install Ubuntu 22.04 for this workflow, preferably under `G:\` on lab
machines:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  hardware\arduinonrf\nrf52\tools\ncs_zigbee\ncp_host.ps1 `
  -Port COM27 -InstallUbuntu -WslDistro Ubuntu `
  -UbuntuLocation G:\WSL\ArduinoNRF-Ubuntu
```

On WSL2, attach the Zephyr NCP USB device with `usbipd-win` instead of using the
legacy `/dev/ttyS27` COM-port mapping. For board1 in the current lab state,
`usbipd list` reports the NCP USB CDC device as `2fe3:0001` on BUSID `4-3`.
Run the attach command from an elevated PowerShell:

```powershell
winget install --id dorssel.usbipd-win --source winget

powershell -NoProfile -ExecutionPolicy Bypass -File `
  hardware\arduinonrf\nrf52\tools\ncs_zigbee\ncp_host.ps1 `
  -Port COM27 -UsbBusId 4-3 -WslDistro Ubuntu `
  -WslTty /dev/ttyACM0 -AttachUsb
```

After attach, start the official host through the Linux CDC ACM device:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  hardware\arduinonrf\nrf52\tools\ncs_zigbee\ncp_host.ps1 `
  -Port COM27 -WslTty /dev/ttyACM0 -RunSimpleGw
```

If WSL2 USB/IP attaches the Zephyr CDC ACM device and then drops it with
`connection reset by peer`, use a WSL1 distro for direct Windows COM-port
mapping instead:

```powershell
wsl --install Ubuntu-22.04 --name ArduinoNRF-Ubuntu1 `
  --location G:\WSL\ArduinoNRF-Ubuntu1 --version 1 --no-launch --web-download

powershell -NoProfile -ExecutionPolicy Bypass -File `
  hardware\arduinonrf\nrf52\tools\ncs_zigbee\ncp_host.ps1 `
  -Port COM27 -WslDistro ArduinoNRF-Ubuntu1 -WslTty /dev/ttyS27 `
  -RunSimpleGw
```

For a bounded smoke run, add `-RunSeconds 20`. The official host invocation is
environment-variable based: `NCP_SLAVE_PTY=<linux-serial-port>
./application/simple_gw/simple_gw`.

## Existing working path: external CC2530

Use this when you want an Arduino sketch to keep running on the nRF52840 while
Zigbee / 802.15.4 traffic is handled by an external module.

- **Driver library:** **[ArduinoNRF-Zigbee](https://github.com/dunknowcoding/ArduinoNRF-Zigbee)**
  - Install it separately in the Arduino IDE.
  - Provides raw 802.15.4 send / receive / promiscuous sniff through the
    `CC2530Radio` API over `Serial1`.
  - Bundles the SDCC transceiver firmware for the module.
- **Built-in flasher:** [`libraries/CCDebugger/`](../../hardware/arduinonrf/nrf52/libraries/CCDebugger/)
  turns the nRF52840 into a TI CC2530 programmer.

Current lab state:

- board1-board5 each have a CC2530 module connected.
- The NiusZigbee path remains usable while the official onboard path is being
  added.

### Quick use

1. Install the **ArduinoNRF board package** and the **ArduinoNRF-Zigbee** library.
2. Wire **DD/DC/RST -> D8/D9/D10** for flashing and **UART -> D0/D1** for
   runtime, all at **3.3 V**.
3. Run *Examples -> ArduinoNRF-Zigbee -> CC2530_FlashFirmware* once.
4. Use *CC2530_Info / CC2530_Sniffer / CC2530_Link*.

### Notes

- 3.3 V only; the CC2530 is not 5 V tolerant.
- This external path can coexist with Arduino BLE because the nRF52840 RADIO is
  not used by the CC2530 module.
- This path is not the same thing as official onboard ZBOSS firmware.

## `libraries/Zigbee` status

[`libraries/Zigbee/`](../../hardware/arduinonrf/nrf52/libraries/Zigbee/) remains
an experimental Arduino API placeholder. Until a future phase explicitly wires a
real backend, `begin()` returns `ZIGBEE_NOT_VENDORED` and `isAvailable()` returns
`false`.

Do not use this skeleton as evidence that official nRF-native Zigbee is already
linked into Arduino sketches. The official path is the nCS sidecar flow above.
