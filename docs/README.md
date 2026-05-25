# ArduinoNRF documentation

All project documentation lives here. Start with the [project README](../README.md) for the overview and quick start.

## Getting started & status

- [VALIDATION.md](VALIDATION.md) — what's verified on real hardware, timing, and how to reproduce it
- [release/RELEASE_NOTES_v0.0.1.md](release/RELEASE_NOTES_v0.0.1.md) — release notes
- [release/README.md](release/README.md) · [release/PRE_RELEASE_CHECKLIST.md](release/PRE_RELEASE_CHECKLIST.md) — release flow

## Uploading

- [uploads/hands_free_upload.md](uploads/hands_free_upload.md) — button-less serial-DFU on the maintenance CDC (the default path)
- [uploads/double_reset_upload.md](uploads/double_reset_upload.md) — manual double-reset bootloader entry
- [uploads/swd_only_upload.md](uploads/swd_only_upload.md) — flashing via an SWD probe
- [platform/UPLOAD_BEHAVIOR.md](platform/UPLOAD_BEHAVIOR.md) — upload policy and behavior
- [platform/USB_1200_TOUCH_V1_FIX.md](platform/USB_1200_TOUCH_V1_FIX.md) — the firmware fixes that make hands-free upload work
- [bootloaders/README.md](bootloaders/README.md) — bootloader families

## Debugging

- [platform/ARDUINO_IDE2_USB_GDBSTUB.md](platform/ARDUINO_IDE2_USB_GDBSTUB.md) — single-cable USB-CDC GDB-stub debugging in Arduino IDE 2
- [examples/vscode/](examples/vscode/) — sample `launch.json` / `tasks.json`

## Boards

- [boards/README.md](boards/README.md) — per-board reference index
- [platform/BOARD_SUPPORT_STATUS.md](platform/BOARD_SUPPORT_STATUS.md) — support matrix
- [platform/BOARD_SUPPORT_NOTES.md](platform/BOARD_SUPPORT_NOTES.md) · [platform/BOARD_FAMILY_MATRIX.md](platform/BOARD_FAMILY_MATRIX.md) — family notes & matrix

## Platform reference (capabilities & truth)

- [platform/HARDWARE_CAPABILITIES.md](platform/HARDWARE_CAPABILITIES.md) — what the core exposes
- [platform/PWM_BEHAVIOR.md](platform/PWM_BEHAVIOR.md) · [platform/PWM_TIMER_BOUNDARIES.md](platform/PWM_TIMER_BOUNDARIES.md) — PWM model & limits
- [platform/POWER_ADC_NOTES.md](platform/POWER_ADC_NOTES.md) — ADC / battery-sense behavior
- [platform/BLE_WIFI_BOUNDARIES.md](platform/BLE_WIFI_BOUNDARIES.md) — BLE/WiFi facade boundaries
- [platform/clock_ble_low_power_menu.md](platform/clock_ble_low_power_menu.md) · [platform/storage_backend_menu.md](platform/storage_backend_menu.md) — build menus
- [platform/REFERENCE_COMPARISON.md](platform/REFERENCE_COMPARISON.md) — comparison vs `pdcook/nRFMicro-Arduino-Core`
- [platform/HARDWARE_CAPABILITIES.md](platform/HARDWARE_CAPABILITIES.md), [platform/BOARD_IMAGE_EVIDENCE.md](platform/BOARD_IMAGE_EVIDENCE.md), [platform/DEVBOARD_INCREMENTAL_GATES.md](platform/DEVBOARD_INCREMENTAL_GATES.md) — evidence & gating
