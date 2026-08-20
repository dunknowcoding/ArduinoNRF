<div align="center">

# ArduinoNRF

### An Arduino core for budget nRF52840 clones — with *hands-free uploads* and *single-cable source debugging*

[![platform](https://img.shields.io/badge/platform-Arduino-00979D)](https://www.arduino.cc/)
[![mcu](https://img.shields.io/badge/MCU-nRF52840%20%2F%20nRF52833-0a7bbb)](https://www.nordicsemi.com/)
[![boards](https://img.shields.io/badge/boards-10-success)](#-supported-boards)
[![version](https://img.shields.io/badge/version-0.3.16-blue)](https://github.com/dunknowcoding/ArduinoNRF/releases/tag/v0.3.16)
[![upload](https://img.shields.io/badge/upload-no%20button%2C%20one%20cable-brightgreen)](#-hands-free-uploads)
[![debug](https://img.shields.io/badge/debug-USB%20CDC%20GDB%20stub-orange)](#-single-cable-debugging)
[![ble](https://img.shields.io/badge/BLE-NimBLE%20GATT%20verified-blueviolet)](#-bluetooth-low-energy-nimble)

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
| 🧩 | **10 board definitions, one package** | ProMicro (*verified on hardware*), nice!nano v2, SuperMini, nRFMicro, XIAO, nRF52833/nRF52840 dev boards and more — installed from one Board Manager URL. |
| 📋 | **Truth-oriented metadata** | ADC, PWM, BLE and bus capabilities are documented as *verified*, not aspirational. No silent overclaiming. |
| 🛡️ | **Robust upload pipeline** | Double-click-safe, coexists with a live debug session, rejects the wrong COM with a clear message, and caches slow port scans for speed. |
| 🥋 | **TaichiUSB — own USB stack** | A clean-room nRF52840 USB device stack (**not TinyUSB**): enumeration runs from the USBD ISR, so the COM port survives whatever your `setup()`/`loop()` does. |

---

## 🚀 Quick start

### 1. Install the core

In **Arduino IDE → Settings → Additional Boards Manager URLs**, add:

```raw
https://raw.githubusercontent.com/dunknowcoding/ArduinoNRF/main/package_arduinonrf_index.json
```

Then open **Boards Manager**, search **ArduinoNRF**, and install. (CLI: `arduino-cli core install arduinonrf:nrf52 --additional-urls <url above>`.)

ArduinoNRF is also listed on the [Arduino unofficial third-party boards wiki](https://github.com/arduino/Arduino/wiki/Unofficial-list-of-3rd-party-boards-support-urls).

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
- **UF2-aware** — on Windows, `Auto-detect` prefers the selected board's mounted UF2 drive when it is present, and falls back only to the requested serial-DFU path.
- **Multi-board safe** — UF2 volumes are matched to the selected upload COM by stable USB identity, so two `NICENANO` drives do not collide.
- **Fast** — slow Windows PnP enumerations are cached during the stable pre-touch window (~3 s saved per upload).

See **[docs/uploads/hands_free_upload.md](docs/uploads/hands_free_upload.md)** and **[docs/platform/UPLOAD_BEHAVIOR.md](docs/platform/UPLOAD_BEHAVIOR.md)**.

### Bootloader layout cheat sheet (`INFO_UF2.TXT` → IDE menu)

When the board is in UF2/DFU mode, open **`INFO_UF2.TXT`** on **that board's** drive (do not rely on a fixed drive letter if several clones are plugged in). Use the **`SoftDevice`** line to pick **Tools → Bootloader / DFU** — not the `UF2 Bootloader X.Y.Z` version line alone (the same version string appears on more than one layout).

| `INFO_UF2.TXT` **`SoftDevice`** | Typical **`UF2 Bootloader`** lines seen online *(cross-check only)* | App start | **Tools → Bootloader / DFU** *(ProMicro / nice!nano family)* |
|---|---|---:|---|
| `S140 version 6.1.1` or other **S140 6.x** | `0.6.0`, `0.7.0`, `0.8.0`, `0.11.0`, … | `0x26000` | **Auto-detect upload, SoftDevice S140 v6 layout (0x26000)** — or explicit **UF2 mass storage, SoftDevice S140 v6 layout (0x26000)** / **Serial DFU, SoftDevice S140 v6 layout (0x26000)** |
| **`not found`** | `0.6.0`, `0.7.0`, … *(no-SoftDevice / MBR-only builds)* | `0x1000` | **Auto-detect upload, no SoftDevice / MBR only (0x1000)** — or explicit **UF2 mass storage, no SoftDevice / MBR only (0x1000)** / **Serial DFU, no SoftDevice / MBR only (0x1000)** |
| **S140 7.x** / legacy S140 strings | varies | `0x27000` | **UF2 mass storage, SoftDevice S140 v7 / legacy layout (0x27000)** — or **Serial DFU, SoftDevice S140 v7 / legacy layout (0x27000)** *(no ProMicro auto-detect default for this layout)* |

> 💡 **Example:** `SoftDevice: not found` + `UF2 Bootloader 0.6.0` → choose a **`(0x1000)`** entry, not `(0x26000)`.  
> 💡 **Manual UF2 drag-and-drop** bypasses layout guards — use this table before copying.  
> 📀 Full notes (Burn Bootloader images, update packages, recovery): **[docs/bootloaders/README.md](docs/bootloaders/README.md)**.

## 🐞 Single-cable debugging

A small GDB stub compiled into the firmware uses the Cortex-M **DebugMonitor** exception plus the **FPB** (hardware breakpoints) and **DWT** (watchpoints). A host bridge proxies Arduino IDE 2's `cortex-debug` over the maintenance USB-CDC port, so you get a full debug experience **without any external probe**:

✅ breakpoints · ✅ single-step (in/over/out) · ✅ registers & memory · ✅ data watchpoints · ✅ pause/halt · ✅ peripheral (SVD) reads · ✅ user `Serial` keeps working concurrently on the second CDC

> Verified end-to-end on the AliExpress ProMicro nRF52840 clone. Boards that expose real SWD pads can also use the classic OpenOCD/CMSIS-DAP route.

See **[docs/platform/ARDUINO_IDE2_USB_GDBSTUB.md](docs/platform/ARDUINO_IDE2_USB_GDBSTUB.md)**.

---

## 🔵 Bluetooth Low Energy (NimBLE)

`libraries/NimBLE/` vendors the **Apache Mynewt NimBLE** host *and* link-layer
controller and runs them on a bare-metal cooperative port (no SoftDevice, no
RTOS) — the sketch just calls `NimBLE::poll()` from `loop()`. The board is a
real BLE peripheral that a phone or PC can connect to and fully discover.

Verified on hardware (AliExpress ProMicro nRF52840):

- ✅ Advertising, connection, and **MTU exchange** (256 B)
- ✅ **Full GATT discovery** — services, characteristics, and descriptors — from
  **Windows** (bleak / WinRT), **Android** (nRF Connect), and **board-to-board**
  (a second board acting as GATT central)
- ✅ Standard **GAP (0x1800)** + **GATT (0x1801)** services plus a **Nordic UART**
  service, with notify / write characteristics
- ✅ USB-CDC `Serial` keeps working concurrently for logging

```cpp
#include <NimBLE.h>
void setup() { NimBLE::begin("MyBoard"); NimBLE::startAdvertising(); }
void loop()  { NimBLE::poll(); }   // pump the host + controller
```

The board advertises a **Nordic UART Service**, so exchanging data is as simple
as a wireless `Serial`:

```cpp
NimBLE::write("hello\n");                       // board -> central (TX notify)
NimBLE::onReceive([](const uint8_t *d, size_t n) {  // central -> board (RX write)
  Serial.write(d, n);
});
```

Start from **`examples/NimBLESmoke`** to confirm the stack is up, then
**`examples/BLESend`** (push data to a phone/PC) and **`examples/BLEReceive`**
(receive data from one) — both drive the Nordic UART service and work with the
free **nRF Connect** app. **Thread (OpenThread) mesh runs on the same radio** via
the separate [NiusThread](https://github.com/dunknowcoding/ArduinoNRF-Thread)
library (RADIO is exclusive: BLE *or* Thread per sketch). CC310 crypto runs in
the separate [NiusCrypto](https://github.com/dunknowcoding/ArduinoNRF-Crypto)
library; official own-radio Zigbee is moving to an nCS Zigbee R23 sidecar
firmware path, while **working Arduino-sketch Zigbee is available today via an
external CC2530 module** (see the capability table below).

---

## 🧩 Supported boards

Identities were re-checked against the upstream `Adafruit_nRF52_Bootloader` `board.h` files, the Seeed and pdcook Arduino cores, and `joric/nrfmicro`. See **[docs/COMPATIBILITY.md](docs/COMPATIBILITY.md)** for the full audit and per-board change log.

> ⚠️ In this revision, only the **AliExpress ProMicro nRF52840** upload/debug path is verified end-to-end on physical hardware. The other packaged board definitions are derived from the current variants, upstream identities, and smoke-test truth; some remain marked *experimental* in `boards.txt`.

| Board | Bootloader VID:PID | Pipeline | Status |
|---|---|---|---|
| **AliExpress ProMicro nRF52840** | `0x239A:0x00B3` | UF2 + Adafruit serial DFU | ✅ **Verified on hardware** (hands-free upload + single-cable debug) |
| nice!nano v2 | `0x239A:0x00B3` | UF2 + Adafruit serial DFU | Identity corrected; same pipeline as ProMicro |
| SuperMini nRF52840 | `0x239A:0x00B3` | UF2 + Adafruit serial DFU | Ships nice!nano bootloader; LED on P0.15 |
| nRFMicro | `0x1209:0x5284` | Adafruit serial DFU | joric open-hardware; some DIY units carry nice!nano IDs instead |
| Seeed XIAO nRF52840 (+ Sense) | `0x2886:0x0044` / `0x0045` | Adafruit serial DFU (Seeed) | Identity corrected, UF2 = `XIAO-BOOT` |
| Makerdiary Pitaya Go | `0x2886:0xF00E` | Adafruit serial DFU | Identity corrected, UF2 = `PITAYAGO` |
| Mini nRF52840 (AliExpress) | `auto` | varies | ⚠️ No canonical identity — auto-detect |
| Generic nRF52840 Dev Board | `auto` | varies | ⚠️ Official Nordic DK uses **SWD via J-Link**, not USB DFU |
| Generic nRF52833 Development Board | `auto` | varies | ⚠️ Seller identity varies; packaged, but not yet re-verified on hardware in this revision |
| nRF52840 USB Dongle (PCA10059) | `0x1915:0x521F` | **Nordic Open DFU** | ⚠️ **Use Nordic `nrfutil` / nRF Connect — this pipeline does not apply** |

Per-board reference notes live in **[docs/boards/](docs/boards/)**; the support matrix is in **[docs/platform/BOARD_SUPPORT_STATUS.md](docs/platform/BOARD_SUPPORT_STATUS.md)**.

### 🖥️ OS support

| OS | Hands-free upload | Single-cable debug |
|---|---|---|
| **Windows 10/11** | ✅ Verified (`upload.ps1`) | ✅ Verified (PowerShell bridge) |
| **Linux** (Ubuntu / Debian / Fedora / Arch) | ✅ Implemented via `upload.py` + `adafruit-nrfutil` (untested in this revision) | ✅ Implemented via `usb_gdbstub_bridge.py` (untested) |
| **macOS** (Intel + Apple Silicon) | ✅ Implemented via `upload.py` + `adafruit-nrfutil` (untested) | ✅ Implemented via `usb_gdbstub_bridge.py` (untested) |

Linux/macOS setup: `pip3 install --user adafruit-nrfutil`, then (Linux only) install the shipped udev rules — see **[docs/COMPATIBILITY.md](docs/COMPATIBILITY.md)**.

## ⚙️ Capabilities & honest limits

| Area | Status |
|---|---|
| **GPIO / Serial / SPI / Wire** | ✅ Supported; global `Wire1`/`SPI1` exist but stay disabled on boards without verified secondary-bus pins |
| **ADC** | ✅ SAADC-backed `analogRead`, plus `analogReadVDD()` / `analogReadVDDHDIV5()` |
| **PWM** | ✅ **All 4 PWM modules / 16 channels / 4 independent frequency groups** + per-pin polarity + complementary pairing with software dead-time. Tools → PWM speed picks the default carrier. See [docs/platform/PWM_MULTI_MODULE.md](docs/platform/PWM_MULTI_MODULE.md). |
| **RTC** | ✅ Low-level driver for **RTC0/1/2** (24-bit counter, 4 compares each, overflow IRQ, LFCLK-clocked so it survives sleep). See [docs/platform/RTC_DRIVER.md](docs/platform/RTC_DRIVER.md). |
| **Power** | ✅ **System ON sleep** (WFI/WFE/sleepMs), low-power / constant-latency sub-modes, **DCDC** enable, RAM retention, **SystemOFF** with **GPIO / NFC / USB wake**. See [`NrfPower.h`](hardware/arduinonrf/nrf52/cores/arduino/NrfPower.h) + `examples/PowerSleep`. |
| **NFC-A Tag** | ✅ Type 2 tag emulation (NDEF **URL** / **text** records), auto-collision-resolution via NFCT hardware. See [`NrfNfcTag.h`](hardware/arduinonrf/nrf52/cores/arduino/NrfNfcTag.h) + `examples/NfcTag`. |
| **All small peripherals** | ✅ **TRNG** (`NrfRng`), die **temp sensor** (`NrfTemp`), **watchdog** (`NrfWdt`), **rotary decoder** (`NrfQdec`), **TIMER0–4** w/ 6 CC each (`NrfTimer`), **flash erase/write** (`NrfNvmc`), **PPI** (peripheral-to-peripheral routing) (`NrfPpi`), **EGU/SWI** (software events + IRQ on 6×16 channels) (`NrfEgu`), **analog comparator** (`NrfComp`), **memory watch unit** (`NrfMwu`), **GPIOTE output channels** for PPI use (`NrfGpioteOut`). All in [`NrfPeripherals.h`](hardware/arduinonrf/nrf52/cores/arduino/NrfPeripherals.h) + examples `PeripheralsDemo`, `TimerNvmcPpi`, `EguCompGpiotePpi`. |
| **Media peripherals** | ⚠️ **QSPI** (external NOR flash), **PDM** (MEMS mic), **I2S** (digital audio) drivers exist in [`NrfMediaPeripherals.h`](hardware/arduinonrf/nrf52/cores/arduino/NrfMediaPeripherals.h). API + register sequences complete per nRF52840 PS, but unverified — the reference ProMicro has none of those external chips wired. XIAO Sense (PDM) / Pitaya Go (QSPI) / custom audio boards should work, please test. |
| **BLE (NimBLE)** | ✅ **Working over-the-air BLE.** [`libraries/NimBLE/`](hardware/arduinonrf/nrf52/libraries/NimBLE/) vendors the Apache Mynewt NimBLE host **and** controller on a bare-metal cooperative port. Advertising, connection, MTU exchange, and full GATT service / characteristic / descriptor discovery are **verified on hardware** against Windows (bleak/WinRT), Android (nRF Connect), and board-to-board. See **[§ Bluetooth Low Energy (NimBLE)](#-bluetooth-low-energy-nimble)** and `examples/NimBLESmoke`. |
| **CC310 (crypto)** | ✅ **Working CC310 hardware acceleration** via **[NiusCrypto](https://github.com/dunknowcoding/ArduinoNRF-Crypto)** (CRYS on CryptoCell 310: SHA-256, HMAC, AES-CBC/CTR, ECDSA/ECDH P-256, TRNG; Oberon for AES-GCM; hardware verified). [`libraries/CC310/`](hardware/arduinonrf/nrf52/libraries/CC310/) shim forwards `#include <NrfCC310.h>` (`depends=NiusCrypto`); `CC310Smoke` covers TRNG/hash/HMAC/AES-CTR/ECDSA/ECDH. |
| **Zigbee / 802.15.4** | 🚧 **Official onboard Zigbee is now a sidecar target.** The nRF52840 own-radio path tracks Nordic's nCS Zigbee R23 add-on through `hardware/arduinonrf/nrf52/tools/ncs_zigbee/`; the reference target is a no-SoftDevice ProMicro-class board over SWD, with no bootloader reflashing. ✅ **Working Arduino-sketch path remains the external CC2530 module.** Flash it with the built-in **[`libraries/CCDebugger/`](hardware/arduinonrf/nrf52/libraries/CCDebugger/)**, then drive it over UART with **[ArduinoNRF-Zigbee](https://github.com/dunknowcoding/ArduinoNRF-Zigbee)**. `libraries/Zigbee/` is still an experimental placeholder (`ZIGBEE_NOT_VENDORED`). Guide: **[docs/platform/ZIGBEE.md](docs/platform/ZIGBEE.md)**. |
| **Thread (OpenThread)** | ✅ **Working IPv6 mesh on the nRF52840's own radio** via the separate **[NiusThread](https://github.com/dunknowcoding/ArduinoNRF-Thread)** library: full OpenThread FTD (vendored, with mbedtls) on a bare-metal 802.15.4 RADIO driver — no SoftDevice, no nrfx, no external module. HW-verified two-node mesh (Leader + Child → Router) with bidirectional UDP multicast. The in-package [`libraries/Thread/`](hardware/arduinonrf/nrf52/libraries/Thread/) is now a thin shim that keeps `#include <Thread.h>` working. Guide: **[docs/platform/THREAD.md](docs/platform/THREAD.md)**. |
| **EEPROM** | ✅ Emulated (see EEPROM examples) |
| **USB device stack** | ✅ **TaichiUSB** — self-developed clean-room nRF52840 USBD stack (dual CDC + DFU + PluggableUSB), ISR-driven enumeration so the port survives a blocking sketch, **block-write CDC at ~300 KB/s**; **not TinyUSB**. See [docs/platform/TAICHIUSB.md](docs/platform/TAICHIUSB.md). |
| **Upload** | ✅ Hands-free UF2 or serial-DFU on the maintenance CDC; SWD/OpenOCD also available |
| **Debug** | ✅ USB-CDC GDB stub (no probe) on ProMicro; SWD route for boards with pads |

**Gaps vs. mature Adafruit/Nordic cores:** no SoftDevice menu or Bluefruit/TinyUSB BLE stack; not every clone USB profile is claimed interchangeable. The validated ProMicro path is `uploadmode=usb,bootloader=auto`, with explicit UF2 and explicit serial DFU also verified. Hands-free in-app upload is verified for **both** `usbcdc=enabled` and `usbcdc=disabled`; `usbcdc=enabled` is the default because it also gives you a separate user `Serial` port alongside the upload/maintenance CDC.

---

## 📚 Documentation

Everything beyond this README lives under **[docs/](docs/)** — start at **[docs/README.md](docs/README.md)** for the full index. Highlights:

- 🧪 **[docs/COMPATIBILITY.md](docs/COMPATIBILITY.md)** — supported boards, layouts, and compatibility status
- 🔼 **[docs/uploads/hands_free_upload.md](docs/uploads/hands_free_upload.md)** — hands-free upload, plus the double-reset and SWD-probe fallbacks
- 📀 **[docs/bootloaders/README.md](docs/bootloaders/README.md)** — `INFO_UF2.TXT` version/layout reference and manual UF2 recovery
- 🥋 **[docs/platform/TAICHIUSB.md](docs/platform/TAICHIUSB.md)** — the self-developed (clean-room, not TinyUSB) nRF52840 USB device stack
- 🐞 **[docs/platform/ARDUINO_IDE2_USB_GDBSTUB.md](docs/platform/ARDUINO_IDE2_USB_GDBSTUB.md)** — single-cable debug setup
- 🧩 **[docs/boards/](docs/boards/)** — per-board reference
- 📦 **[GitHub Releases](https://github.com/dunknowcoding/ArduinoNRF/releases)** — release notes

## 🛠️ Repository layout

```raw
hardware/arduinonrf/nrf52/    # the Arduino platform: cores, variants, boards.txt, tools
examples/                     # peripheral, UART/UARTE, BLE send/receive, and USB examples
docs/                         # all documentation (the only place docs live)
package_arduinonrf_index.json # Board Manager index
```

## 🤝 Contributing & license

Issues and PRs welcome at **[github.com/dunknowcoding/ArduinoNRF](https://github.com/dunknowcoding/ArduinoNRF)**. The guiding principle: **document what the hardware verifiably does** — if a capability isn't proven on a board, it isn't claimed.

### License

ArduinoNRF is licensed under the **[Apache License 2.0](LICENSE)** — © 2026 **dunknowcoding (NiusRobotLab)**.

You may use it freely, **including in commercial products**. In return, the license asks you to:

- 🙏 **Credit the original author** — keep the attribution to dunknowcoding (NiusRobotLab); retaining the [`NOTICE`](NOTICE) file (or reproducing its attribution in your product's credits) satisfies this *(Apache §4(d))*.
- 📝 **Document your changes** — any file you modify must carry a prominent notice that you changed it *(Apache §4(b))*.
- 📄 **Ship a copy of the license** with redistributions *(Apache §4(a))*.
- ⚖️ **No patent weaponization** — the work comes with a patent grant that terminates for anyone who starts patent litigation against it *(Apache §3)*.

Vendored third-party code (Apache Mynewt NimBLE, Nordic MDK / CMSIS headers, upload tools) keeps its own license — see [`NOTICE`](NOTICE) and the per-file headers.
