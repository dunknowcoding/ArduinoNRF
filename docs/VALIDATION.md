# Hardware validation status

What has been verified on real hardware, and how to reproduce it. Reference board: **AliExpress ProMicro nRF52840 clone** (nice!nano-compatible bootloader family `0x239A:0x00B3`, UF2 label `NICENANO`).

On this clone the runtime service CDC and the bootloader share the same VID:PID, so VID/PID alone cannot tell host tooling which mode the board is in; the upload pipeline (`upload.ps1`) copes with same-PID-aware detection.

## ✅ Verified

| Capability | Status | Notes |
|---|---|---|
| Hands-free reupload (single cable) | **PASS** | A second `arduino-cli upload` on the same COM needs no manual reset — the 1200-bps touch triggers `SYSRESETREQ` into the bootloader, adafruit-nrfutil streams the image, the board returns to user mode. |
| Single-cable USB-CDC debug (Arduino IDE 2) | **PASS** | Breakpoints (FPB), step in/over/out, registers, memory, data watchpoints (DWT), pause (DebugMonitor), peripheral/SVD reads — no external probe. |
| Dual-CDC concurrency | **PASS** | User `Serial` streams on the user CDC while a debug session runs on the service CDC over the same cable; pause/resume and host→user RX don't wedge the debug link. |
| Upload during a debug session | **PASS** | The debug bridge yields the service COM on an upload request (both before a client connects and mid-session); the halted-stub 1200-touch then reboots to the bootloader and the flash completes. |
| Repeated Upload clicks | **PASS** | A per-port lock makes a duplicate upload fail fast before any touch; firmware is never corrupted. |
| Wrong-port (user CDC) upload | **PASS** | Selecting the user CDC (`MI_02`) is rejected with a clear message; the firmware is untouched. The firmware also ignores a 1200-touch on the user CDC by design. |
| `usbcdc=disabled` in-app upload | **PASS** | Hands-free in-app 1200-touch reboots and flashes on the single service CDC. Verified 3× back-to-back, plus `usbcdc=disabled⇄enabled` transitions in both directions. |
| UF2 upload from DFU mode | **PASS** | `bootloader=auto` and explicit `bootloader=promicro` both match the selected board's UF2 drive and complete `Upload complete`. |
| Explicit serial DFU from DFU mode | **PASS** | `bootloader=promicroserial` enters `Starting Adafruit serial DFU transfer` and completes even while another UF2 drive is mounted. |
| UF2 drive-only helper | **PASS** | `uploadmode=uf2boot` leaves the selected board mounted as UF2 and exits with `Upload skipped`. |
| UF2 layout guard | **PASS** | A mounted `NICENANO` drive reporting `SoftDevice: not found` was rejected when the sketch was compiled for `0x26000` on both UF2 and explicit serial-DFU IDE Upload; recompiling/selecting the no-SoftDevice `0x1000` layout allowed upload to proceed. |
| Multi-board UF2 disambiguation | **PASS** | Two `NICENANO` volumes were mounted simultaneously; board1 matched `J:\` by stable ID and board2 matched `K:\`. Stale COM selection is rejected. |
| SEGGER J-Link SWD upload | **PASS** | `Upload Method -> SWD programmer (SEGGER J-Link)` uses SEGGER `JLink.exe`; board1 flashed a smoke sketch over SWD in ~1.3-2.3 s. |
| Upload Using Programmer, J-Link | **PASS** | `Tools -> Programmer -> SEGGER J-Link (SWD)` plus `Upload Using Programmer` flashed the same smoke sketch through the SEGGER path. |
| Burn Bootloader recipe, J-Link/CMSIS-DAP | **NOT RUN** | Recipe is wired through `niusboot`: J-Link uses SEGGER `JLink.exe`; CMSIS-DAP uses OpenOCD `nrf52_recover`. Not executed during validation because it performs a full chip erase. |
| CC2530 flash via built-in CCDebugger | **PASS** | Two CC2530 modules wired to board1/board2 were detected as `0xA5xx`; the SDCC transceiver firmware was erased, programmed, and read-back verified. |
| CC2530 runtime UART PING | **PASS** | `CC2530Radio.begin(11)` reports firmware `v0.1` and repeated `ping -> PONG` after the NiusZigbee UART resync fix. |
| CC2530 two-node raw 802.15.4 link | **PASS** | `CC2530_Link` on board1 and board2 produced `TX "hello N" ok` and reciprocal `RX (... dBm): hello N` frames on channel 11. |
| board1 J-Link read-only identify | **PASS** | SEGGER J-Link connected to board1 at 3.3 V, detected Cortex-M4 / nRF52840 (`FICR 0x10000100 = 0x00052840`). No erase, recover, bootloader flash, or application write was run. |
| Official Zigbee no-SoftDevice NCP USB build | **PASS** | `-ImageLayout no-softdevice` produced `.ncs-zigbee-work/b/an/pm40/ncp-nosd/zephyr/zephyr.hex`; HEX range `0x1000..0x7190F`; partitions preserve `0x0000..0x0FFF` and `0xE9000..0xFFFFF`. |
| Official Zigbee no-SoftDevice NCP USB flash (board1) | **PASS** | Flashed board1 with J-Link using `flash_zigbee.ps1`; no `recover`, `eraseall`, or bootloader flashing. Readback confirmed `0x00000000` bootloader vectors unchanged and app vectors present at `0x00001000`. Windows enumerated Zephyr USB CDC as `VID_2FE3&PID_0001`, `COM27`. |
| Official ZBOSS NCP Host package prep | **PASS, HOST NOT RUN** | `ncp_host.ps1 -Port COM27 -Download -Extract` downloaded `ncp_host_v3.6.0.zip` from `ncs-zigbee v1.3.0` into `.ncs-zigbee-work/` and found Linux `simple_gw`; WSL 2.7.8 is installed, but the Windows WSL optional component is still missing or pending reboot, so host protocol validation was not run. |
| NiusCrypto CC310 self-test (board1) | **PASS** | `examples/CryptoSelfTest` with vendored CRYS+Oberon: 10/10 KAT vectors, `backend: CC310`, UF2 flash. See [ArduinoNRF-Crypto docs/VALIDATION.md](https://github.com/dunknowcoding/ArduinoNRF-Crypto/blob/main/docs/VALIDATION.md). |
| CC310 shim smoke (board1) | **PASS** | `CC310Smoke`: TRNG, SHA-256, HMAC, AES-CTR, ECDSA via shim → NiusCrypto; `RESULT: OK`. |

## ⚠️ Known limitations

- **Raw `adafruit-nrfutil dfu serial`** is less robust than `arduino-cli upload` on this clone because it lacks the package's port identity checks, retries, stale-COM guard, and **layout guard**.
- **Manual UF2 drag-and-drop** bypasses `upload.ps1`. Match bootloader **layout** (`INFO_UF2.TXT` `SoftDevice` field), not just the version string, before copying sketch or update packages.
- A no-SoftDevice nice!nano-compatible bootloader reports `SoftDevice: not found`
  in `INFO_UF2.TXT`; use a no-SoftDevice menu (`bootloader=autonosd`,
  `bootloader=promicronosduf2`, or `bootloader=promicroserialnosd`, app start
  `0x1000`) for sketches on that bootloader. Uploading a SoftDevice layout can
  appear to succeed while the application does not run; Windows IDE Upload now
  fails fast when the mounted UF2 layout conflicts with the compiled app start
  (UF2 and serial DFU). If USB/COM disappears after a **manual** mismatched copy,
  double-tap RESET (or RST–GND twice on buttonless boards) or recover over SWD.

## ⏱️ Upload timing (measured)

Typical wall-clock for `arduino-cli upload` of a precompiled sketch, ProMicro clone on Windows:

| Phase | Time | Nature |
|---|---|---|
| Pre-touch port/identity checks | ~2.0 s | optimized (PnP snapshot cache; was ~5.1 s) |
| Touch → bootloader enumerated | ~3.7 s | firmware reboot + bootloader USB enum (floor) |
| UF2/DFU transfer | ~6-7 s | bootloader flash/write bound for the small smoke sketches |
| `arduino-cli` port enumeration wrapper | ~9 s | arduino-cli's own scan; the IDE does less with a pre-selected port |

The package-controllable pipeline (`upload.ps1`) runs in ~14–15 s; the rest is firmware/bootloader and arduino-cli floors.

## How to reproduce

### Official onboard Zigbee sidecar environment check

Initial target: physical **board1** (`promicro_nrf52840`) with a SEGGER J-Link
connected over SWD. The first modules do not flash anything; they verify the
sidecar tool entry point, install/build against Nordic's official stack, and
keep all SDK/build output in `.ncs-zigbee-work/`.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  hardware\arduinonrf\nrf52\tools\ncs_zigbee\build_zigbee.ps1 `
  -Board promicro_nrf52840 -Target ncp_usb -CheckOnly
```

Expected:

- The script reports `promicro_nrf52840` and `ncp_usb`.
- It checks for `west`, `cmake`, Python, and `ZEPHYR_SDK_INSTALL_DIR`.
- It creates or reuses `.ncs-zigbee-work/`.
- It does not download, build, flash, recover, erase, or reflash any bootloader.

### Official Zigbee R23 reference build

Validated on Windows with:

- `ncs-zigbee` add-on `v1.3.0`.
- Manifest-resolved nRF Connect SDK `v2.9.2`.
- `west` from the `IronEngineWorld` conda environment.
- Nordic toolchain at `C:\ncs\toolchains\b620d30767`.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  hardware\arduinonrf\nrf52\tools\ncs_zigbee\build_zigbee.ps1 `
  -Board promicro_nrf52840 -Target ncp_usb -Pristine always
```

Result: **PASS** for build-only reference firmware. The wrapper maps the
ArduinoNRF board alias to `nrf52840dk/nrf52840` and builds Nordic's official
Zigbee NCP USB sample with sysbuild and `FILE_SUFFIX=usb`.

Key artifacts:

```text
.ncs-zigbee-work/build/arduinonrf/promicro_nrf52840/ncp_usb/merged.hex
.ncs-zigbee-work/build/arduinonrf/promicro_nrf52840/ncp_usb/dfu_application.zip
.ncs-zigbee-work/build/arduinonrf/promicro_nrf52840/ncp_usb/ncp/zephyr/zephyr.signed.hex
```

Flash status: **NOT RUN** on board1. The reference `merged.hex` starts at
`0x0` and would overwrite an Arduino bootloader. `flash_zigbee.ps1` now rejects
such files by default for `no-softdevice` and `softdevice-s140-v6` layouts.

### Official Zigbee no-SoftDevice NCP USB build

Validated for board1's no-SoftDevice ProMicro/nice!nano-style bootloader:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  hardware\arduinonrf\nrf52\tools\ncs_zigbee\build_zigbee.ps1 `
  -Board promicro_nrf52840 -Target ncp_usb -ImageLayout no-softdevice `
  -Pristine always
```

Result: **PASS** for build-only bootloader-preserving firmware. The wrapper uses
`--no-sysbuild`, disables MCUboot, sets `CONFIG_FLASH_LOAD_OFFSET=0x1000`, and
passes a static Partition Manager layout for the ArduinoNRF no-SoftDevice app
window.

Protected layout:

```text
0x0000..0x0FFF  low bootloader / MBR guard
0x1000..0xDFFFF application
0xE0000..0xE7FFF ZBOSS NVRAM
0xE8000..0xE8FFF ZBOSS product config
0xE9000..0xFFFFF top UF2 bootloader guard
```

Key artifacts:

```text
.ncs-zigbee-work/b/an/pm40/ncp-nosd/zephyr/zephyr.hex
.ncs-zigbee-work/b/an/pm40/ncp-nosd/zephyr/zephyr.bin
```

Dry-run address guard:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  hardware\arduinonrf\nrf52\tools\ncs_zigbee\flash_zigbee.ps1 `
  -Board promicro_nrf52840 -BootloaderLayout no-softdevice `
  -Hex .ncs-zigbee-work\b\an\pm40\ncp-nosd\zephyr\zephyr.hex -DryRun
```

Result: **PASS**. The generated `zephyr.hex` range is `0x1000..0x7190F`, inside
the protected `0x1000..0xE8FFF` application window.

### Official Zigbee no-SoftDevice board1 flash

Validated on physical **board1** with SEGGER J-Link:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  hardware\arduinonrf\nrf52\tools\ncs_zigbee\flash_zigbee.ps1 `
  -Board promicro_nrf52840 -BootloaderLayout no-softdevice `
  -Hex .ncs-zigbee-work\b\an\pm40\ncp-nosd\zephyr\zephyr.hex
```

Result: **PASS**. J-Link reported Program & Verify OK. The script did not run
`recover`, `eraseall`, or any bootloader flashing command.

Post-flash readback:

```text
0x00000000: 20000400 00000A81 00000715 00000A61
0x00001000: 20015E80 000092FD 0005ED71 000092E9 ...
```

The `0x00000000` bootloader vectors match the pre-flash read-only identify step,
and the Zephyr application vector table is present at `0x00001000`.

Windows USB enumeration:

```text
USB\VID_2FE3&PID_0001\EDBBE67D3759AAEA
USB\VID_2FE3&PID_0001&MI_00\...\0000
USB Serial Device (COM27)
```

This confirms the official Zigbee NCP USB firmware is running far enough to
enumerate as a Zephyr USB CDC device on board1. Higher-level NCP host protocol
validation remains the next step.

### Official ZBOSS NCP Host package prep

The matching official host package for `ncs-zigbee v1.3.0` is
`ncp_host_v3.6.0.zip`. It was prepared with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  hardware\arduinonrf\nrf52\tools\ncs_zigbee\ncp_host.ps1 `
  -Port COM27 -Download -Extract
```

Result: **PASS** for package prep. The wrapper downloaded and extracted the
package into `.ncs-zigbee-work/ncp-host/3.6.0/` and confirmed
`application/simple_gw/simple_gw` exists.

The host executable is a 64-bit Linux ELF. It is not a native Windows program.
Current local status:

- `Microsoft.WSL` was installed/updated with winget to WSL `2.7.8`.
- `ncp_host.ps1 -Port COM27` reports
  `Windows optional component: missing or pending reboot`.
- `simple_gw` is present under `.ncs-zigbee-work/ncp-host/3.6.0/src/`.
- The host was not run because Ubuntu/WSL is not active yet.

If the status remains unchanged after reboot, enable WSL from an elevated
PowerShell:

```powershell
wsl --install --no-distribution
```

Then install Ubuntu 22.04 for this workflow, keeping it under `G:\`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  hardware\arduinonrf\nrf52\tools\ncs_zigbee\ncp_host.ps1 `
  -Port COM27 -InstallUbuntu -WslDistro Ubuntu `
  -UbuntuLocation G:\WSL\ArduinoNRF-Ubuntu
```

After installing or enabling Ubuntu/WSL, run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  hardware\arduinonrf\nrf52\tools\ncs_zigbee\ncp_host.ps1 `
  -Port COM27 -RunSimpleGw
```

The wrapper maps `COM27` to `/dev/ttyS27` by default for WSL.

### First flash (manual bootloader entry, once)

If the board is in an unknown state, enter the bootloader manually once: short `RST` to `GND` twice. A UF2 drive appears and the service COM (e.g. `COM3`) becomes visible. Then upload normally.

### Hands-free reupload smoke test

```powershell
arduino-cli compile --fqbn arduinonrf:nrf52:promicro_nrf52840:uploadmode=usb,bootloader=auto,usbcdc=enabled <sketch>
arduino-cli upload  -p COMx --fqbn arduinonrf:nrf52:promicro_nrf52840:uploadmode=usb,bootloader=auto,usbcdc=enabled <sketch>
```

Expected: upload completes with `Soft reset : done - board rebooted into new firmware`. If the board is already in DFU mode, use the current DFU/SERVICE COM shown by Arduino IDE; if it has returned to application mode, use the current service COM. Running the same command again must also succeed hands-free.

### UF2 drive-only smoke test

```powershell
arduino-cli upload -p COMx --fqbn arduinonrf:nrf52:promicro_nrf52840:uploadmode=uf2boot,bootloader=auto <sketch>
```

Expected: the selected board reports `UF2 drive ready`, prints the matched drive, and exits with `Upload skipped : board left in bootloader`.

### Single-cable debug smoke test

Build the `examples/UsbGdbStubBreakpoint` sketch with the `usbgdbstub` build profile and press **Debug** in Arduino IDE 2 (or attach `cortex-debug` to the bridge's TCP port). The board halts at the `bkpt #0` in `setup()`; stepping, registers, and breakpoints work over the single USB cable. See [platform/ARDUINO_IDE2_USB_GDBSTUB.md](platform/ARDUINO_IDE2_USB_GDBSTUB.md).
