# Board Image Evidence

Date: 2026-05-04

## Evidence tiers

- `official-image-backed`: vendor documentation provides front/back imagery, pinout diagrams, or schematics that directly support the board notes.
- `community-image-backed`: community-maintained photos or pinouts are strong enough to support board-shape and pad-layout claims, but not all clone variants.
- `user-image-backed`: a real board image supplied during validation supports claims for that user's exact hardware, not necessarily the whole clone family.
- `package-modeled`: current support is derived mainly from the repository variant, package metadata, or family-level inference.

## Current inventory

- `promicro_nrf52840`: `user-image-backed`. The current user board is a ProMicro-class nRF52840 clone with a four-pad SWD cluster labeled `VDD`, `DIO`, `CLK`, and `GND`. This is strong enough to model pads-only SWD for that board class, but clone-to-clone variation still exists.
- `nicenano_v2`: `official-image-backed`. nice!nano documentation provides pinout and schematic material that supports the modeled `VDDHDIV5` battery path, `EXT_VCC` control, and center-pad secondary-bus mapping.
- `xiao_nrf52840`: `official-image-backed` for the XIAO family. Seeed documentation provides front/back indication diagrams and explicit SWD access guidance. The repository target remains `XIAO-like`, so clone-specific deltas still need care.
- `nrfmicro_nrf52840`: `community-image-backed`. The nRFMicro wiki and reference-core materials provide high-resolution photos and pinouts that support VBAT, `EXT_VCC`, and extra-pad mapping.
- `supermini_nrf52840`: `community-image-backed` to `reference-pinout-backed`. Current confidence comes from reference-core pinouts and nRFMicro-community materials, but not yet from a single official vendor image set for every AliExpress clone.
- `usb_dongle_nrf52840`: `official-image-backed` for dongle-class reference hardware. MakerDiary material supports USB-dongle assumptions, but the repository target is still generic rather than tied to one commercial SKU.
- `mini_nrf52840`: `package-modeled`. No equally strong front/back or schematic-backed public source has been locked to this repository target yet.
- `devboard_nrf52840`: `package-modeled`. This target remains a generic development-board abstraction rather than a single branded board.
- `devboard_nrf52833`: `package-modeled`. This target remains a generic development-board abstraction rather than a single branded board.
- `pitaya_go_nrf52840`: `package-modeled`. The current repository model is stronger than the current public image evidence gathered in-session.

## Use of this file

- Upgrade board documentation from `package-modeled` to stronger claims only when the board's evidence tier supports it.
- Keep clone-family claims separate from exact-board claims.
- Prefer image-backed or schematic-backed facts when deciding SWD access style, battery path, and exposed auxiliary pads.
