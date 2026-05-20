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

### Button-less reflash on a single USB cable

Two consecutive compile + upload cycles work on the same single SERVICE
CDC COM (e.g. COM3) without a manual reset between them. The clone shares
USB VID:PID `0x239A:0x00B3` between its runtime and bootloader, which used
to make the 1200 bps touch + serial DFU flow stall. The fix lives in three
coordinated firmware patches in `NrfUsbd.cpp` (EP0 OUT EasyDMA trigger, EP0
direction routing by internal state, and a sticky touch-confirm latch). See
`docs/platform/USB_1200_TOUCH_V1_FIX.md` for the full root-cause writeup.

### One-click Debug in Arduino IDE 2

Select **Build Options -> USB CDC GDB stub for IDE 2 Debug** and press the
Debug button. The host-side TCP<->serial bridge launches automatically (no
separate terminal), cortex-debug attaches over `localhost:3335`, and you get
breakpoints (Cortex-M4 FPB, 6 hardware breakpoints), single-step, and the
Variables / Registers panes. See `docs/platform/ARDUINO_IDE2_USB_GDBSTUB.md`.

### Clean upload console

The upload output is now quiet by default: the NiusRobotLab banner once,
an ASCII progress bar `[=========>          ]  50%  Streaming firmware`, and
a final result line. All internal diagnostics are gated behind verbose
upload (Arduino IDE 2 preference, or `NIUS_UPLOAD_VERBOSE=1`). No more
double-printed lines, no red mirror, no Python tracebacks in the normal
path.

### Concise Tools menus

| Menu | Options |
|---|---|
| Bootloader | Auto-detect (recommended), Serial DFU no BLE `[promicroserialnosd]`, Serial DFU with BLE `[promicroserial]`, UF2 drive ... |
| USB Serial (CDC) | Enabled / Disabled |
| USB DFU | Enabled / Disabled |
| Upload via | USB bootloader / CMSIS-DAP SWD |
| Flash | 1 MB internal |
| SRAM | 256 KB |
| Build Options | Release, Debug symbols, USB CDC GDB stub for IDE 2 Debug |

Each Bootloader entry carries its FQBN key in `[brackets]` so docs that
reference `bootloader=promicroserialnosd` line up with the dropdown.

### Portable tooling

`upload.ps1` no longer hard-codes any developer-specific paths. It locates
`adafruit-nrfutil` from `NIUS_ADAFRUIT_NRFUTIL_EXE`, then PATH, then the
standard python.org per-user Scripts directories, preferring a non-Conda
install. Output is pure ASCII so it renders correctly regardless of the
Windows console codepage.

## Supported boards

AliExpress ProMicro nRF52840 (reference), nice!nano v2, SuperMini
nRF52840, nRFMicro nRF52840, Mini nRF52840, XIAO-like nRF52840, Generic
nRF52840 / nRF52833 dev boards, Pitaya Go nRF52840, nRF52840 USB Dongle.

## Requirements

- Arduino IDE 2.x (or arduino-cli)
- For serial DFU upload: `adafruit-nrfutil` (`pip install adafruit-nrfutil`)
- Tool dependencies (arm-none-eabi-gcc, dfu-util, openocd) are fetched
  automatically by Boards Manager.
