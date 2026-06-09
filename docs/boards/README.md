# Board Documents

The ProMicro nRF52840 is the lead board and has its own document. The other
packaged boards share the same package model; their identities and notes are
summarized below, and the authoritative pin/peripheral truth lives in
`boards.txt` and the per-board `variants/` files.

## Board index

- [promicro_nrf52840.md](promicro_nrf52840.md) — AliExpress ProMicro nRF52840; the lead board with end-to-end hands-free upload, single-cable debug, and BLE GATT (Windows/Android/board-to-board) verified on hardware.
- **nicenano_v2** — nice!nano v2; same UF2 / Adafruit serial-DFU family, secondary buses modeled, not yet re-verified on hardware here.
- **supermini_nrf52840** — SuperMini nRF52840; nice!nano-class bootloader identity, secondary buses modeled.
- **nrfmicro_nrf52840** — nRFMicro; pid.codes identity, secondary buses modeled.
- **mini_nrf52840** — generic AliExpress mini module; identity varies by seller.
- **xiao_nrf52840** — Seeed XIAO nRF52840 / Sense family model.
- **pitaya_go_nrf52840** — Makerdiary Pitaya Go model.
- **devboard_nrf52840** — generic nRF52840 dev board; official Nordic DK route is SWD/J-Link.
- **devboard_nrf52833** — generic nRF52833 dev board; SWD-first package model.
- **usb_dongle_nrf52840** — PCA10059 USB dongle; Nordic Open DFU, not the package's UF2 / Adafruit serial-DFU path.

## Evidence shorthand

- `verified`: checked on real hardware.
- `modeled`: derived from the current package, variants, and smoke tests.
- `reference-core`: derived from a trusted reference implementation rather than direct hardware proof.
- `partial`: coherent package behavior exists, but the board-level evidence chain is incomplete.

**Related**: clone ProMicro / single-cable USB upload and dual-CDC + USB-CDC GDB-stub debugging are covered in **[`docs/VALIDATION.md`](../VALIDATION.md)** and **[`docs/platform/ARDUINO_IDE2_USB_GDBSTUB.md`](../platform/ARDUINO_IDE2_USB_GDBSTUB.md)**.
