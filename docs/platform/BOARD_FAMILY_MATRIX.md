# Board Family Matrix

Date: 2026-05-04

| Family | Boards | Current emphasis |
| --- | --- | --- |
| `promicro-compatible` | `promicro_nrf52840`, `nicenano_v2`, `supermini_nrf52840`, `nrfmicro_nrf52840` | Most important recovery area; these boards have the largest gap versus mature reference cores. |
| `mini-module` | `mini_nrf52840` | Generic battery-capable module with QSPI and no modeled secondary bus. |
| `xiao-like` | `xiao_nrf52840` | XIAO-style clone model with QSPI and no battery sense in the current package. |
| `devboard` | `devboard_nrf52840`, `devboard_nrf52833` | Generic development boards used as representative compile and smoke-test targets. |
| `handheld` | `pitaya_go_nrf52840` | Feature-rich handheld model with battery, QSPI, WiFi coprocessor, and IMU metadata. |
| `usb-dongle` | `usb_dongle_nrf52840` | Compact native-USB target with SWD pads-only debug metadata. |
