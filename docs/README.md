# ArduinoNRF documentation

All project documentation lives here. Start with the [project README](../README.md) for the overview and quick start.

## Getting started & status

- [COMPATIBILITY.md](COMPATIBILITY.md) — **OS support matrix (Windows / Linux / macOS) and real-board identity audit**
- [VALIDATION.md](VALIDATION.md) — what's verified on real hardware, timing, and how to reproduce it
- [release/RELEASE_NOTES_v0.1.0.md](release/RELEASE_NOTES_v0.1.0.md) — current release notes
- [release/RELEASE_NOTES_v0.0.1.md](release/RELEASE_NOTES_v0.0.1.md) — initial release notes
- [release/README.md](release/README.md) · [release/PRE_RELEASE_CHECKLIST.md](release/PRE_RELEASE_CHECKLIST.md) — release flow

## Uploading

- [uploads/hands_free_upload.md](uploads/hands_free_upload.md) — button-less serial-DFU on the maintenance CDC (the default verified path is the ProMicro clone's `promicroserialnosd` workflow)
- [uploads/double_reset_upload.md](uploads/double_reset_upload.md) — manual double-reset bootloader entry
- [uploads/swd_only_upload.md](uploads/swd_only_upload.md) — flashing via an SWD probe
- [platform/UPLOAD_BEHAVIOR.md](platform/UPLOAD_BEHAVIOR.md) — upload policy and behavior
- [platform/USB_1200_TOUCH_V1_FIX.md](platform/USB_1200_TOUCH_V1_FIX.md) — the firmware fixes that make hands-free upload work
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

## Multi-session roadmaps (large vendoring efforts)

- [platform/NIMBLE_INTEGRATION_PLAN.md](platform/NIMBLE_INTEGRATION_PLAN.md) — full BLE via Apache Mynewt's NimBLE (no SoftDevice). Skeleton at [`libraries/NimBLE/`](../hardware/arduinonrf/nrf52/libraries/NimBLE/).
- [platform/CC310_INTEGRATION_PLAN.md](platform/CC310_INTEGRATION_PLAN.md) — CryptoCell 310 hardware crypto. Skeleton at [`libraries/CC310/`](../hardware/arduinonrf/nrf52/libraries/CC310/).
- [platform/ZIGBEE_INTEGRATION_PLAN.md](platform/ZIGBEE_INTEGRATION_PLAN.md) — IEEE 802.15.4 + Zboss. Skeleton at [`libraries/Zigbee/`](../hardware/arduinonrf/nrf52/libraries/Zigbee/).
- [platform/THREAD_INTEGRATION_PLAN.md](platform/THREAD_INTEGRATION_PLAN.md) — Thread (OpenThread) IPv6 mesh, the protocol Matter / Apple Home / Google Home use. Skeleton at [`libraries/Thread/`](../hardware/arduinonrf/nrf52/libraries/Thread/).

## Platform reference (capabilities & truth)

- [platform/HARDWARE_CAPABILITIES.md](platform/HARDWARE_CAPABILITIES.md) — what the core exposes
- [platform/PWM_BEHAVIOR.md](platform/PWM_BEHAVIOR.md) · [platform/PWM_TIMER_BOUNDARIES.md](platform/PWM_TIMER_BOUNDARIES.md) — PWM model & limits (legacy single-module notes)
- [platform/POWER_ADC_NOTES.md](platform/POWER_ADC_NOTES.md) — ADC / battery-sense behavior
- [platform/BLE_WIFI_BOUNDARIES.md](platform/BLE_WIFI_BOUNDARIES.md) — BLE/WiFi facade boundaries (current advertising-only stub)
- [platform/clock_ble_low_power_menu.md](platform/clock_ble_low_power_menu.md) · [platform/storage_backend_menu.md](platform/storage_backend_menu.md) — build menus
- [platform/REFERENCE_COMPARISON.md](platform/REFERENCE_COMPARISON.md) — comparison vs `pdcook/nRFMicro-Arduino-Core`
- [platform/BOARD_IMAGE_EVIDENCE.md](platform/BOARD_IMAGE_EVIDENCE.md) · [platform/DEVBOARD_INCREMENTAL_GATES.md](platform/DEVBOARD_INCREMENTAL_GATES.md) — evidence & gating
