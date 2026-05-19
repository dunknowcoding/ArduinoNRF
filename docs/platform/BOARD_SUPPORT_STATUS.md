# Board Support Status

Date: 2026-05-04

Evidence levels used in this repository:

- `verified`: checked on real hardware.
- `modeled`: derived from current variant, package, and smoke-test truth.
- `reference-core`: derived from a specific reference implementation rather than direct hardware proof.
- `partial`: some package behavior is validated, but board-level evidence is incomplete.

| Board | Family | Evidence | Pin map | Battery model | Upload profile | Secondary buses |
| --- | --- | --- | --- | --- | --- | --- |
| AliExpress ProMicro nRF52840 | promicro-compatible | modeled | partial | partial | partial | modeled absent |
| nice!nano v2 | promicro-compatible | modeled + reference-core | partial | partial | partial | reference-modeled present |
| SuperMini nRF52840 | promicro-compatible | modeled + reference-core | partial | partial | partial | reference-modeled present |
| nRFMicro nRF52840 | promicro-compatible | modeled + reference-core | partial | partial | partial | reference-modeled present |
| Mini nRF52840 | mini-module | modeled | partial | partial | partial | modeled absent |
| XIAO-like nRF52840 | xiao-like | modeled | partial | not applicable or partial | partial | modeled absent |
| Generic nRF52840 Development Board | devboard | modeled | partial | not applicable | partial | modeled absent |
| Generic nRF52833 Development Board | devboard | modeled | partial | not applicable | partial | modeled absent |
| Pitaya Go nRF52840 | handheld | modeled | partial | partial | partial | modeled absent |
| nRF52840 USB Dongle | usb-dongle | modeled | partial | not applicable | partial | modeled absent |

Notes:

- `partial` means the package exposes a coherent current model, but the repository still lacks a full real-board evidence chain.
- `reference-modeled present` means the current package now exposes secondary-bus pins because the reference core and local pin numbering align, but this is not yet a real-hardware verification claim.
- No packaged board is currently marked `verified` for hands-free upload on real hardware.
