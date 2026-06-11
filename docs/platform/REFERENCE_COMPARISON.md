# Reference Comparison

Reference: `pdcook/nRFMicro-Arduino-Core` (and, for ecosystem depth, the
Adafruit nRF52 core it derives from). Last revised: 2026-06-10.

## What this package now does better

- **Hands-free upload** over the existing CDC: VID/PID-aware 1200-bps touch,
  port remap, UF2 *and* serial-DFU paths, concurrency-safe (`upload.ps1`),
  verified for `usbcdc=enabled` and `disabled`.
- **On-board debugging with no probe**: a USB-CDC GDB stub (DebugMonitor)
  with breakpoints, stepping, Pause, DWT watchpoints — single cable, works
  from Arduino IDE 2 and VS Code. The reference cores have nothing
  comparable.
- **Working wireless stacks on the chip's own radio, without a SoftDevice**:
  - BLE via a vendored NimBLE host + controller (advertising, connection,
    MTU, GATT discovery verified over the air);
  - **Thread (OpenThread FTD)** via the separate
    [NiusThread](https://github.com/dunknowcoding/ArduinoNRF-Thread) library
    — two-node mesh, UDP and CoAP hardware-verified;
  - raw 802.15.4 + sniffing via an external CC2530 and the built-in
    CC-Debugger flasher ([ZIGBEE.md](ZIGBEE.md)).
- **Complete discrete-peripheral driver coverage** (see
  [PERIPHERAL_DRIVER_COVERAGE.md](PERIPHERAL_DRIVER_COVERAGE.md)): RTC,
  TIMER, PPI, EGU, COMP/LPCOMP, QDEC, MWU, NVMC, ECB/CCM/AAR, UICR,
  proprietary radio, and more — plus multi-module PWM (16 channels / 4
  frequency groups).
- **Explicit truth surfaces** (`NrfBoardInfo`, `NrfBoardSupportStatus`,
  `NrfBoardPowerInfo`, `NrfSystemProfile`) and validation sketches that
  check the declared package truth on real hardware.
- Real global `Wire1` / `SPI1` objects on boards that route them.

## What the reference cores still have that this package lacks

- **SoftDevice menu integration** and everything that rides on it
  (Bluefruit API surface, Nordic-blessed BLE qualification path).
- **TinyUSB** ecosystem depth (MSC, HID, MIDI composite devices) — this
  core's USB stack is CDC-focused (multi-CDC + DFU interfaces).
- Mature pinout documentation, board images, and the long tail of
  community-tested board definitions.
- Broad multi-board hardware validation: here only the AliExpress ProMicro
  path is verified end-to-end on silicon; other board definitions are
  derived truth ([BOARD_SUPPORT_STATUS.md](BOARD_SUPPORT_STATUS.md)).

## Current conclusion

The original gap — "tells the truth but can't do much" — has inverted in
several areas: upload, probe-less debug, and own-radio networking (BLE,
Thread) now exceed what the reference cores offer, without a SoftDevice.
The remaining deficits are ecosystem breadth (SoftDevice/TinyUSB/Bluefruit
compatibility) and per-board hardware validation coverage.
