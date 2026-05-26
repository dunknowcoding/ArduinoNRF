<div align="center">

# ArduinoNRF

### An Arduino core for budget nRF52840 clones — with *hands-free uploads* and *single-cable source debugging*

[![platform](https://img.shields.io/badge/platform-Arduino-00979D)](https://www.arduino.cc/)
[![mcu](https://img.shields.io/badge/MCU-nRF52840%20%2F%20nRF52833-0a7bbb)](https://www.nordicsemi.com/)
[![boards](https://img.shields.io/badge/boards-10-success)](#-supported-boards)
[![version](https://img.shields.io/badge/version-0.0.1-blue)](docs/release/RELEASE_NOTES_v0.0.1.md)
[![upload](https://img.shields.io/badge/upload-no%20button%2C%20one%20cable-brightgreen)](#-hands-free-uploads)
[![debug](https://img.shields.io/badge/debug-USB%20CDC%20GDB%20stub-orange)](#-single-cable-debugging)

*Flash and debug an AliExpress ProMicro nRF52840 — the kind with no reset button and no SWD header — over a single USB cable, straight from the Arduino IDE.*

</div>

---

## Why this core exists

The cheap nRF52840 clones (AliExpress "ProMicro nRF52840", SuperMini, nRFMicro, …) are fantastic value, but they usually ship without workable board library for Arduino IDE 2.x, which makes those boards extremely difficult to use and debug. Widely-used ProMicro even comes without reset buttons and usable SWD pins (it does has the pads). **NiusRobotLab** developed this easy-to-use, Arduino IDE compatible third-party board library that makes every upload automatically as a double-tap-reset dance, and source-level debugging possible merely using **single USB cable**. **No debugger needed any more!** If you would like to support our work, test this repo with your own nRF board then tell me any issues or possible fix.

## ✨ Highlights

| | Feature | What it means for you |
|---|---|---|
| 🔌 | **Hands-free uploads** | Click **Upload**. The firmware reboots itself into the bootloader over the USB-CDC maintenance port — **no button, no double-reset, no jumper**. |
| 🐞 | **Single-cable debugging** | Click **Debug** in Arduino IDE 2 and get **breakpoints, step in/over/out, registers, memory, watchpoints and pause** — all over the *same* USB cable, with **no external SWD/J-Link probe**. |
| 🧩 | **10 boards, one package** | ProMicro (*verified*), nice!nano v2, SuperMini, nRFMicro, XIAO-like, dev boards and more — installed from one Board Manager URL. |
| 📋 | **Truth-oriented metadata** | ADC, PWM, BLE and bus capabilities are documented as *verified*, not aspirational. No silent overclaiming. |
| 🛡️ | **Robust upload pipeline** | Double-click-safe, coexists with a live debug session, rejects the wrong COM with a clear message, and caches slow port scans for speed. |

---

## 🚀 Quick start

### 1. Install the core

In **Arduino IDE → Settings → Additional Boards Manager URLs**, add:

```raw
https://raw.githubusercontent.com/dunknowcoding/ArduinoNRF/main/package_arduinonrf_index.json
```

Then open **Boards Manager**, search **ArduinoNRF**, and install. (CLI: `arduino-cli core install arduinonrf:nrf52 --additional-urls <url above>`.)

### 2. Pick your board

**Tools → Board → ArduinoNRF nRF52** → e.g. *AliExpress ProMicro nRF52840*. Select the board's COM port under **Tools → Port**.

> 💡 With `usbcdc=enabled` the board shows **two** COM ports: a **user** port (your `Serial`) and a **service/maintenance** port. Use the **service** port (usually the lower COM index / `MI_00`) for uploads and debugging — picking the user port is rejected with a clear message.

### 3. Upload

Hit **Upload**. The first time the board is in application mode, the core's 1200-bps touch reboots it into the bootloader automatically and the new firmware streams in. No button press. 🎉

### 4. Debug (optional, one cable)

Select the `usbgdbstub` build profile, open `examples → UsbGdbStubBreakpoint`, and press **Debug**. Set breakpoints in the gutter and step through your code — the GDB stub lives in the firmware and talks over the maintenance CDC. See **[docs/platform/ARDUINO_IDE2_USB_GDBSTUB.md](docs/platform/ARDUINO_IDE2_USB_GDBSTUB.md)**.

---

## 🔌 Hands-free uploads

Most clone cores can only enter the bootloader via a physical double-reset. ArduinoNRF's firmware watches the **maintenance CDC** for the classic 1200-bps "touch" and reboots itself into the UF2/serial-DFU bootloader via `SYSRESETREQ` — so `arduino-cli upload` (and the IDE Upload button) work with **no hands on the board**.

The host-side pipeline (`upload.ps1`) is hardened against the messy real world:

- **Double-click safe** — a per-port lock makes a second Upload fail fast *before* any touch, so two uploads can never interleave and corrupt flash.
- **Coexists with debugging** — if a debug session holds the port, the upload signals the debug bridge to release it, then proceeds.
- **Wrong-port guard** — selecting the *user* CDC instead of the service CDC is rejected with an actionable message; your firmware is never touched.
- **Fast** — slow Windows PnP enumerations are cached during the stable pre-touch window (~3 s saved per upload).

See **[docs/uploads/hands_free_upload.md](docs/uploads/hands_free_upload.md)** and **[docs/platform/UPLOAD_BEHAVIOR.md](docs/platform/UPLOAD_BEHAVIOR.md)**.

## 🐞 Single-cable debugging

A small GDB stub compiled into the firmware uses the Cortex-M **DebugMonitor** exception plus the **FPB** (hardware breakpoints) and **DWT** (watchpoints). A host bridge proxies Arduino IDE 2's `cortex-debug` over the maintenance USB-CDC port, so you get a full debug experience **without any external probe**:

✅ breakpoints · ✅ single-step (in/over/out) · ✅ registers & memory · ✅ data watchpoints · ✅ pause/halt · ✅ peripheral (SVD) reads · ✅ user `Serial` keeps working concurrently on the second CDC

> Verified end-to-end on the AliExpress ProMicro nRF52840 clone. Boards that expose real SWD pads can also use the classic OpenOCD/CMSIS-DAP route.

See **[docs/platform/ARDUINO_IDE2_USB_GDBSTUB.md](docs/platform/ARDUINO_IDE2_USB_GDBSTUB.md)**.

---

## 🧩 Supported boards

| Board | MCU | Notes |
|---|---|---|
| **AliExpress ProMicro nRF52840** | nRF52840 | Primary target — hands-free upload **and** single-cable debug verified |
| nice!nano v2 | nRF52840 | Reference-backed LFCLK + secondary-bus metadata |
| SuperMini nRF52840 | nRF52840 | Check VCC behavior under USB power |
| nRFMicro nRF52840 | nRF52840 | Reference-backed secondary-bus metadata |
| Mini nRF52840 | nRF52840 | |
| XIAO-like nRF52840 | nRF52840 | |
| Generic nRF52840 Dev Board | nRF52840 | |
| Generic nRF52833 Dev Board | nRF52833 | |
| Pitaya Go nRF52840 | nRF52840 | |
| nRF52840 USB Dongle | nRF52840 | |

Per-board reference docs live in **[docs/boards/](docs/boards/)**; the support matrix is in **[docs/platform/BOARD_SUPPORT_STATUS.md](docs/platform/BOARD_SUPPORT_STATUS.md)**.

## ⚙️ Capabilities & honest limits

| Area | Status |
|---|---|
| **GPIO / Serial / SPI / Wire** | ✅ Supported; global `Wire1`/`SPI1` exist but stay disabled on boards without verified secondary-bus pins |
| **ADC** | ✅ SAADC-backed `analogRead`, plus `analogReadVDD()` / `analogReadVDDHDIV5()` |
| **PWM** | ⚠️ Single shared `PWM0` group — up to 4 routed outputs sharing one frequency domain |
| **BLE** | ⚠️ Advertising-only self-hosted facade — **not** a full connection-oriented GATT stack |
| **EEPROM** | ✅ Emulated (see EEPROM examples) |
| **Upload** | ✅ Hands-free serial-DFU on the maintenance CDC; SWD/OpenOCD also available |
| **Debug** | ✅ USB-CDC GDB stub (no probe) on ProMicro; SWD route for boards with pads |

**Gaps vs. mature Adafruit/Nordic cores:** no SoftDevice menu or Bluefruit/TinyUSB BLE stack; not every clone USB profile is claimed interchangeable (the validated path is `promicroserialnosd`). With `usbcdc=disabled` the board stays uploadable *from the bootloader* but its in-app touch is unreliable — **keep `usbcdc=enabled`** for the hands-free workflow.

---

## 📚 Documentation

Everything beyond this README lives under **[docs/](docs/)** — start at **[docs/README.md](docs/README.md)** for the full index. Highlights:

- 🧪 **[docs/VALIDATION.md](docs/VALIDATION.md)** — what's been tested on real hardware, and how to reproduce it
- 🔼 **[docs/uploads/](docs/uploads/)** — hands-free, double-reset, and SWD-only upload routes
- 🐞 **[docs/platform/ARDUINO_IDE2_USB_GDBSTUB.md](docs/platform/ARDUINO_IDE2_USB_GDBSTUB.md)** — single-cable debug setup
- 🧷 **[docs/platform/USB_1200_TOUCH_V1_FIX.md](docs/platform/USB_1200_TOUCH_V1_FIX.md)** — the firmware fixes behind hands-free upload
- 🧩 **[docs/boards/](docs/boards/)** — per-board reference
- 📦 **[docs/release/](docs/release/)** — release flow and notes

## 🛠️ Repository layout

```raw
hardware/arduinonrf/nrf52/    # the Arduino platform: cores, variants, boards.txt, tools
examples/                     # ~50 examples incl. UsbGdbStub* and capability self-tests
docs/                         # all documentation (the only place docs live)
scripts/ , tools/             # build, release, and hardware-validation helpers
package_arduinonrf_index.json # Board Manager index
```

## 🤝 Contributing & license

Issues and PRs welcome at **[github.com/dunknowcoding/ArduinoNRF](https://github.com/dunknowcoding/ArduinoNRF)**. The guiding principle: **document what the hardware verifiably does** — if a capability isn't proven on a board, it isn't claimed.

> ℹ️ A formal open-source license has not yet been added to this repository. Until one is present, treat the code as "all rights reserved" and ask before redistributing.
