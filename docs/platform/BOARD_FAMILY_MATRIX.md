# Board Family Matrix

Last revised: 2026-06-10.

| Family | Boards | Current emphasis |
| --- | --- | --- |
| `promicro-compatible` | `promicro_nrf52840`, `nicenano_v2`, `supermini_nrf52840`, `nrfmicro_nrf52840` | The verified flagship family: upload, USB, debug, BLE (NimBLE), Thread (NiusThread) and the peripheral drivers are all hardware-validated on the AliExpress ProMicro. Siblings share the variant model but remain derived truth. |
| `mini-module` | `mini_nrf52840` | Generic battery-capable module with QSPI and no modeled secondary bus. |
| `xiao-like` | `xiao_nrf52840` | XIAO-style clone model with QSPI; LED/analog pins re-audited 2026-06 (no battery sense modeled in the current package). |
| `devboard` | `devboard_nrf52840`, `devboard_nrf52833` | Generic development boards used as representative compile and smoke-test targets; the 52833 exercises the smaller-memory build. |
| `handheld` | `pitaya_go_nrf52840` | Feature-rich handheld model with battery, QSPI, WiFi coprocessor, and IMU metadata. |
| `usb-dongle` | `usb_dongle_nrf52840` | Compact native-USB target (PCA10059-style) with SWD pads-only debug metadata; LED pins re-audited 2026-06. |

Per-board verification state lives in
[BOARD_SUPPORT_STATUS.md](BOARD_SUPPORT_STATUS.md); identity sources and the
audit trail are in [../COMPATIBILITY.md](../COMPATIBILITY.md).
