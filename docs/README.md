# ArduinoNRF documentation

All project documentation lives here. Start with the [project README](../README.md) for the overview and quick start.

## Getting started & status

- [COMPATIBILITY.md](COMPATIBILITY.md) — **OS support matrix (Windows / Linux / macOS) and real-board identity audit**
- [VALIDATION.md](VALIDATION.md) — what's verified on real hardware, timing, and how to reproduce it
- Release notes: the [GitHub Releases page](https://github.com/dunknowcoding/ArduinoNRF/releases)

## Uploading

- [uploads/hands_free_upload.md](uploads/hands_free_upload.md) — button-less UF2 / serial-DFU on the maintenance CDC, UF2 drive-only mode, stale-port protection, and SWD-probe fallbacks
- [platform/UPLOAD_BEHAVIOR.md](platform/UPLOAD_BEHAVIOR.md) — upload policy and behavior
- [bootloaders/README.md](bootloaders/README.md) — bootloader families

## Debugging

- [platform/ARDUINO_IDE2_USB_GDBSTUB.md](platform/ARDUINO_IDE2_USB_GDBSTUB.md) — single-cable USB-CDC GDB-stub debugging in Arduino IDE 2
- [examples/vscode/README.md](examples/vscode/README.md) — sample `launch.json` / `tasks.json` for VS Code + `cortex-debug`

## Boards

- [boards/README.md](boards/README.md) — per-board reference index
- [platform/BOARD_SUPPORT_STATUS.md](platform/BOARD_SUPPORT_STATUS.md) — support matrix
- [platform/BOARD_SUPPORT_NOTES.md](platform/BOARD_SUPPORT_NOTES.md) · [platform/BOARD_FAMILY_MATRIX.md](platform/BOARD_FAMILY_MATRIX.md) — family notes & matrix

## Peripherals (subsystem drivers)

- [platform/PWM_MULTI_MODULE.md](platform/PWM_MULTI_MODULE.md) — 4-module / 16-channel PWM facade, per-pin frequency, polarity, complementary pairs
- [platform/RTC_DRIVER.md](platform/RTC_DRIVER.md) — low-level driver for RTC0/1/2 (compare + overflow IRQs)
- [`cores/arduino/NrfPower.h`](../hardware/arduinonrf/nrf52/cores/arduino/NrfPower.h) — power management: System ON sleep (WFI/WFE), low-power / constant-latency sub-modes, DCDC, RAM retention, SystemOFF + GPIO / NFC / USB wake sources
- [`cores/arduino/NrfNfcTag.h`](../hardware/arduinonrf/nrf52/cores/arduino/NrfNfcTag.h) — NFC-A Type 2 tag emulation (NDEF URI / text), field detect IRQ, read count
- [`cores/arduino/NrfPeripherals.h`](../hardware/arduinonrf/nrf52/cores/arduino/NrfPeripherals.h) — **NrfRng** (TRNG), **NrfWdt** (watchdog), **NrfTemp** (die temp), **NrfQdec** (rotary encoder), **NrfTimer** (TIMER0–4), **NrfNvmc** (flash erase/write), **NrfPpi** (peripheral routing), **NrfEgu** (software events + SWI on 6 channels), **NrfComp** (analog comparator), **NrfMwu** (memory watch unit), **NrfGpioteOut** (output channels for PPI use)
- [`cores/arduino/NrfMediaPeripherals.h`](../hardware/arduinonrf/nrf52/cores/arduino/NrfMediaPeripherals.h) — **NrfQspi** (external NOR flash), **NrfPdm** (MEMS mic), **NrfI2s** (digital audio). API complete; not verified on the reference ProMicro (no external flash/mic/codec wired) — boards with that hardware need their own verification pass.

## Wireless stacks

- **NimBLE (`libraries/NimBLE/`) — working.** The vendored Apache Mynewt NimBLE
  host+controller runs on a bare-metal cooperative port. Advertising,
  connections, MTU exchange, and full GATT service/characteristic/descriptor
  discovery are verified on hardware against Windows (bleak/WinRT), Android
  (nRF Connect), and board-to-board. Exchange data over the built-in Nordic
  UART service with `NimBLE::write()` / `NimBLE::onReceive()`. See the BLE
  section of the [project README](../README.md) and the `NimBLESmoke`,
  `BLESend`, and `BLEReceive` examples.
- **Zigbee / 802.15.4 — working via an external CC2530 module.** Flash the module
  with the built-in CC-Debugger ([`libraries/CCDebugger/`](../hardware/arduinonrf/nrf52/libraries/CCDebugger/),
  no external programmer) and drive it with the separate
  **[ArduinoNRF-Zigbee](https://github.com/dunknowcoding/ArduinoNRF-Zigbee)**
  library. Full guide: **[platform/ZIGBEE.md](platform/ZIGBEE.md)**.
- **Thread (OpenThread) — working IPv6 mesh on the nRF52840's own radio.**
  Full OpenThread FTD on a bare-metal 802.15.4 driver, in the separate
  **[NiusThread](https://github.com/dunknowcoding/ArduinoNRF-Thread)** library
  (the in-package `libraries/Thread/` is a compatibility shim). HW-verified
  two-node mesh with UDP. Full guide: **[platform/THREAD.md](platform/THREAD.md)**.
- **CC310 (CryptoCell 310) — working hardware crypto on the nRF52840.** SHA-256,
  AES-CBC/CTR, ECDSA/ECDH P-256, TRNG on the accelerator (CRYS runtime) plus
  AES-GCM (Oberon), in the separate
  **[NiusCrypto](https://github.com/dunknowcoding/ArduinoNRF-Crypto)** library
  (the in-package [`libraries/CC310/`](../hardware/arduinonrf/nrf52/libraries/CC310/)
  is a compatibility shim). HW-verified on ProMicro. Nordic binaries are fetched
  locally — see NiusCrypto's `docs/VENDORING.md`.
- **Not yet vendored:** the nRF *own-radio* Zigbee stack (`libraries/Zigbee/` —
  Zboss not in-tree). It exposes its API but returns `ZIGBEE_NOT_VENDORED` until
  the runtime is added.

## Platform reference (capabilities & truth)

- [platform/HARDWARE_CAPABILITIES.md](platform/HARDWARE_CAPABILITIES.md) — what the core exposes
- [platform/PWM_BEHAVIOR.md](platform/PWM_BEHAVIOR.md) · [platform/PWM_TIMER_BOUNDARIES.md](platform/PWM_TIMER_BOUNDARIES.md) — PWM model & frequency/resolution limits (multi-module summary; full model in [platform/PWM_MULTI_MODULE.md](platform/PWM_MULTI_MODULE.md))
- [platform/POWER_ADC_NOTES.md](platform/POWER_ADC_NOTES.md) — ADC / battery-sense behavior
- [platform/BLE_WIFI_BOUNDARIES.md](platform/BLE_WIFI_BOUNDARIES.md) — BLE/WiFi facade boundaries (BLE is now a full NimBLE stack; WiFi remains out of scope on nRF52)
- [platform/THREAD.md](platform/THREAD.md) — Thread (OpenThread) mesh on the native radio via the NiusThread library
- [platform/ZIGBEE.md](platform/ZIGBEE.md) — Zigbee / raw 802.15.4 via an external CC2530 module
- [platform/REFERENCE_COMPARISON.md](platform/REFERENCE_COMPARISON.md) — comparison vs `pdcook/nRFMicro-Arduino-Core`
