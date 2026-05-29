# Board Documents

This directory contains one document per packaged board.

Each board document states the current evidence level, upload behavior, battery and power-path model, secondary-bus truth, BLE/LFCLK status, and major caveats.

## Board index

- [promicro_nrf52840.md](promicro_nrf52840.md) — AliExpress ProMicro nRF52840; the only board with end-to-end hands-free upload + single-cable debug verified on hardware in this revision.
- [nicenano_v2.md](nicenano_v2.md) — nice!nano v2; same Adafruit serial-DFU family, secondary buses modeled, not yet re-verified on hardware here.
- [supermini_nrf52840.md](supermini_nrf52840.md) — SuperMini nRF52840; nice!nano-class bootloader identity, secondary buses modeled.
- [nrfmicro_nrf52840.md](nrfmicro_nrf52840.md) — nRFMicro; pid.codes identity, secondary buses modeled.
- [mini_nrf52840.md](mini_nrf52840.md) — generic AliExpress mini module; identity varies by seller.
- [xiao_nrf52840.md](xiao_nrf52840.md) — Seeed XIAO nRF52840 / Sense family model.
- [pitaya_go_nrf52840.md](pitaya_go_nrf52840.md) — Makerdiary Pitaya Go model.
- [devboard_nrf52840.md](devboard_nrf52840.md) — generic nRF52840 dev board; official Nordic DK route is SWD/J-Link.
- [devboard_nrf52833.md](devboard_nrf52833.md) — generic nRF52833 dev board; SWD-first package model.
- [usb_dongle_nrf52840.md](usb_dongle_nrf52840.md) — PCA10059 USB dongle; Nordic Open DFU, not the package's Adafruit serial-DFU path.

## Evidence shorthand

- `verified`: checked on real hardware.
- `modeled`: derived from the current package, variants, and smoke tests.
- `reference-core`: derived from a trusted reference implementation rather than direct hardware proof.
- `partial`: coherent package behavior exists, but the board-level evidence chain is incomplete.

**Related**: clone ProMicro / single-cable USB upload and dual-CDC + USB-CDC GDB-stub debugging are covered in **[`docs/VALIDATION.md`](../VALIDATION.md)** and **[`docs/platform/ARDUINO_IDE2_USB_GDBSTUB.md`](../platform/ARDUINO_IDE2_USB_GDBSTUB.md)**.
