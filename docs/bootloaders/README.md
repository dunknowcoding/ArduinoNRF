# Bootloader Notes

Date: 2026-05-29

## Bootloader families in this package

### Adafruit serial DFU

This is the default hands-free upload family for the package's USB-DFU boards. On Windows the package uses `upload.ps1`; on Linux/macOS it uses `upload.py` plus `adafruit-nrfutil`.

Boards packaged on this family include the validated AliExpress ProMicro nRF52840 path plus the modeled nice!nano v2, SuperMini, nRFMicro, XIAO, and Pitaya Go definitions. See [../uploads/hands_free_upload.md](../uploads/hands_free_upload.md) and [../COMPATIBILITY.md](../COMPATIBILITY.md).

### UF2

Some board menus still carry UF2 metadata and legacy/manual bootloader options. Those paths remain useful for recovery and manual drag-and-drop workflows, but the repository's currently verified no-button path is the ProMicro clone's Adafruit serial DFU flow rather than a generic UF2 claim.

### Nordic Open DFU

The `usb_dongle_nrf52840` target (PCA10059) uses Nordic Open DFU, not Adafruit serial DFU. Use Nordic tooling such as `nrfutil` or nRF Connect for Desktop rather than the package's default `niusdfu` flow for that board.

### SWD / J-Link / OpenOCD

Generic dev boards, official Nordic DK-style hardware, and any board exposing usable pads can be flashed over SWD. The package supports application flashing over SWD / OpenOCD, but it does not currently provide a bootburn-style workflow to install or replace bootloaders.

## Current package assumption

The package assumes either USB DFU or SWD-based upload depending on the board definition. It does not currently bundle or manage bootloader flashing through a bootburn-style workflow.
