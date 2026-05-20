# Release Notes - v0.0.1

First public release of the **ArduinoNRF** board library for nRF52840 /
nRF52833 boards, with first-class support for the AliExpress ProMicro
nRF52840 clone.

## Install (Arduino IDE 2.x)

1. **File -> Preferences -> Additional Boards Manager URLs**, add:
   ```
   https://raw.githubusercontent.com/dunknowcoding/ArduinoNRF/main/package_arduinonrf_index.json
   ```
2. **Tools -> Board -> Boards Manager**, search **ArduinoNRF**, click Install.
3. Select **Tools -> Board -> ArduinoNRF -> AliExpress ProMicro nRF52840**.

## Highlights

### Button-less, one-shot reflash on a single USB cable

Two consecutive compile + upload cycles work on the same single SERVICE
CDC COM (e.g. COM3) without a manual reset between them. The clone shares
USB VID:PID `0x239A:0x00B3` between its runtime and bootloader, which used
to make the 1200 bps touch + serial DFU flow stall. The fix lives in three
coordinated firmware patches in `NrfUsbd.cpp` (EP0 OUT EasyDMA trigger, EP0
direction routing by internal state, and a sticky touch-confirm latch), plus
a wildcard serial-DFU FWID so the package matches the no-SoftDevice
bootloader on the first attempt. See
`docs/platform/USB_1200_TOUCH_V1_FIX.md`.

### Fast, byte-accurate upload console

- The progress bar tracks **real transferred bytes** (one DFU frame = 512 B)
  and steps through `0-10-20-...-100` with live KB counts, e.g.
  `NIUS  ==========>...........  50%  Uploading  40/79.5 KB`.
- The 1200 bps touch runs on a runspace (not a cold `Start-Job` process) and
  COM-presence polling uses the instant `GetPortNames()` instead of slow WMI,
  so the board enters the bootloader on the first touch and the whole upload
  is ~30% faster end to end.
- Output stays quiet by default: a slant `NiusRobotLab` banner once, the
  progress bar, then a short summary (total time + soft-reset confirmation).
  All internal diagnostics are gated behind verbose upload
  (`NIUS_UPLOAD_VERBOSE=1`). No double-printed lines, no Python tracebacks in
  the normal path. Pure ASCII, so it renders under any console codepage.

### One-click Debug in Arduino IDE 2 (single USB cable)

Select **Build Options -> USB CDC GDB stub for IDE 2 Debug** and press the
Debug button. A native launcher EXE (`usb_gdbstub_server.exe`) stands in for
openocd - it must be a real `.exe` because modern Node refuses to spawn
`.cmd`/`.bat` files, so cortex-debug could never launch the old wrapper. It
parses the gdb port cortex-debug assigns and starts the host-side
TCP<->serial bridge on it; the bridge flushes its "Listening on port" line so
cortex-debug attaches without timing out, then forwards the firmware's GDB
stub. You get breakpoints (Cortex-M4 FPB, 6 hardware breakpoints),
single-step, and the Variables / Registers panes. A stale bridge from a
previous session is evicted automatically. See
`docs/platform/ARDUINO_IDE2_USB_GDBSTUB.md`.

### Concise Tools menus

| Menu | Options |
|---|---|
| Bootloader | Auto-detect, Serial DFU no BLE `[promicroserialnosd]`, Serial DFU with BLE `[promicroserial]`, UF2 drive ... |
| USB Serial (CDC) | Enabled / Disabled |
| USB DFU | Enabled / Disabled |
| Upload via | USB bootloader / CMSIS-DAP SWD |
| Flash | 1 MB internal |
| SRAM | 256 KB |
| Build Options | Release, Debug symbols, USB CDC GDB stub for IDE 2 Debug |

### Portable tooling

`upload.ps1` hard-codes no developer-specific paths. `adafruit-nrfutil` is
bundled as a Boards-Manager tool (no Conda / pip needed); if you prefer your
own, it is also located from `NIUS_ADAFRUIT_NRFUTIL_EXE`, then PATH, then the
python.org per-user Scripts directories.

## Supported boards

AliExpress ProMicro nRF52840 (reference), nice!nano v2, SuperMini
nRF52840, nRFMicro nRF52840, Mini nRF52840, XIAO-like nRF52840, Generic
nRF52840 / nRF52833 dev boards, Pitaya Go nRF52840, nRF52840 USB Dongle.

## Requirements / notes

- Arduino IDE 2.x (or arduino-cli).
- The bundled `adafruit-nrfutil` ships a **Windows** binary in this release;
  the macOS/Linux binary will be added in a later release (on those hosts,
  install `adafruit-nrfutil` via pip and point `NIUS_ADAFRUIT_NRFUTIL_EXE`
  at it for now).
- Tool dependencies (arm-none-eabi-gcc, dfu-util, openocd) are fetched
  automatically by Boards Manager.
