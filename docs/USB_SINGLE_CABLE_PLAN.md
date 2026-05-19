# 单 USB 线、无按键 — 排障与演进计划

本文档对应克隆板（ProMicro / nice!nano v2 兼容 bootloader）在 **仅一条 USB、无物理按键** 约束下的目标架构、现象分析与分阶段工作项。实施过程中在此更新勾选状态与结论。

**相关文档**：双 CDC 维护口/用户口分工、USB GDB stub 与 IDE 集成任务清单见 [`DUAL_CDC_AND_USB_DEBUG_ROADMAP.md`](DUAL_CDC_AND_USB_DEBUG_ROADMAP.md)。**增量真机验收顺序（闸门 G0–G4）**见 [`DEVBOARD_INCREMENTAL_GATES.md`](platform/DEVBOARD_INCREMENTAL_GATES.md)。

---

## 关键要点与易踩坑（必读）

下列条目来自真机排障与代码审查，**后续设计请默认遵守**，避免重复踩坑。

### ProMicro 克隆 / nice!nano v2 兼容串口 DFU bootloader（本仓库验证基线）

| 项目 | 值 / 行为 | 说明 |
|------|-----------|------|
| Bootloader VID:PID | `0x239A:0x00B3` | Windows 常显示 nice!nano |
| **Application flash 起始（AppStart）** | **`0x1000`** | 与本克隆 **串口 DFU / promicroserialnosd** 菜单一致；**不要用 0x26000 / 0x27000** 套在此克隆上（除非更换 bootloader） |
| 软件复位目标 | `GPREGRET = 0x57` | full bootloader 目标；V1 修复后实板运行态 1200 bps touch 可稳定触发该路径，见 [`platform/USB_1200_TOUCH_V1_FIX.md`](platform/USB_1200_TOUCH_V1_FIX.md) |
| 上传后 PnP | 可能仍显示 **同一 PID** | `promicroserialnosd` 下运行态 PID 为 **`0x00B3`**，与 bootloader **相同**，单靠 VID/PID **无法区分** 阶段；`upload.ps1` 的 `runtimeSharesUploadIdentity` 路径会跳过 PnP 判别直接走 DFU |
| 跳过上传后校验 | `$env:NIUS_SKIP_POST_VERIFY='1'` | 传输成功但 Windows 未立刻识别应用 PID 时避免脚本卡死；**勿**用嵌套 `powershell -Command "$env:..."` 丢变量 |
| Poll-only USB | `NRF_USBD_POLL_ONLY=1`（nosd 菜单） | 关闭 USBD NVIC、依赖 `yield`/驱动 `poll()`；EP0 OUT 数据阶段需手动触发 `TASKS_STARTEPOUT[0]`，详见 V1 修复文档 |

### `NrfUsbd` / 控制传输（曾导致「描述符请求失败」）

| 项目 | 结论 |
|------|------|
| EP0 IN **STATUS 阶段** | nRF52 上 **IN 数据阶段在总线上完成后**，须在 `EVENTS_EP0DATADONE`（SETUP 方向为 IN）路径上 **`TASKS_EP0STATUS`** 收尾；遗漏则 Windows 报 **描述符请求失败 / VID_0000** |
| GET_DESCRIPTOR **长度 > 64** | 须 **按 MPS 分包**，总长为 **64 整数倍** 时按规定发送 **ZLP**；源在 `controlInBuffer_` 内时用 **`memmove`** 避免重叠 |
| 误回 bootloader | 曾有无来源 `detachRequested`；现为 **`detachRequestMagic_ == 0xD37ACAFE`** 且带 **`detachCause_`** 才允许 DFU 复位；无效请求不得污染 SRAM 诊断字 |

### 无 SWD 诊断（SRAM 约定）

| 地址 | 用途 |
|------|------|
| `0x20004000` | `Reset_Handler` 等现有诊断标记 |
| `0x20004004` | reset-cause / detach-cause 上报（`ResetCauseReporter`、stub 路径协作时注意勿覆盖） |

### 自动化调试 / Agent 中断上传与 COM「占用」

会话里的调试步骤包括：**强行终止卡在 DFU 的上传**、`usb_gdbstub_bridge` / TcpSmoke **短时拉起后又结束进程** 等。若 **`upload.ps1`、adafruit-nrfutil、桥接脚本或 Arduino CLI** 在 **COM 已打开但未走完收尾（Dispose/Close）** 时被掐断，Windows 常会短时表现为 **「对端口 COMx 的访问被拒绝」**——多数是 **句柄释放滞后或子进程残留**，与「固件逻辑改坏了独占规则」不是一回事。

建议依次：**关掉 Arduino IDE 串口监视器** → **任务管理器结束残留的 `powershell`、`arduino-cli`、`adafruit-nrfutil`、`python`**（若为 nrfutil 拉起）→ **等待十余秒**；仍无效则 **拔插 USB**。再用 **`scripts/test_serial_quick.ps1 COMx`** 验证可独占打开后再上传。

### 脚本入口（硬件）

- 最小上传冒烟：`scripts/hardware_upload_minimal_usb.ps1`（`-MinimalUsb`、`-DualCdcUsbGdbStub`、`UsbdDiagStage`；加 **`-UseArduinoCiConfig`** 时走与本仓库 CI 相同的 **`arduino-cli --config-file .arduino-ci.yaml`** + sketchbook junction）
- 全自动上传（可选自动选 COM）：`scripts/auto_upload_promicro_minimal.ps1`（内部调用 `hardware_upload_minimal_usb.ps1`）
- COM 能否独占打开：`scripts/test_serial_quick.ps1 COMx`
- `arduino-cli` 与本仓库硬件：`scripts/arduino_cli_with_repo.ps1`（`Get-ArduinoCliRepoConfigArgs -EnsureJunction`）
- Windows 串口枚举（Python / conda **`IronEngineWorld`**）：`scripts/win_serial_inventory.py`（见 `conda run -n IronEngineWorld python scripts\win_serial_inventory.py`）
- 本地软件闭环自检（CI 矩阵可选 + `usb_port_helpers` + 双 CDC FQBN 编译，**不含 DFU 上传**）：`scripts/verify_platform_local.ps1`
- 主机端口 / 桥接实机步骤 H：`scripts/hw_verify_step_host_bridge.ps1`（说明见 `docs/platform/DEVBOARD_INCREMENTAL_GATES.md` §步骤 H）
- 真机闸门清单：`docs/platform/DEVBOARD_INCREMENTAL_GATES.md`
- Reset-cause 两阶段：`scripts/hardware_reset_cause_diag.ps1`
- 无硬件 CI：`scripts/usb_single_cable_ci.ps1`

---

## 自动化 CI（无硬件）

仓库脚本：`scripts/usb_single_cable_ci.ps1`

- 通过 Sketchbook **junction** 强制使用本仓库内的 `hardware/arduinonrf/nrf52` 编译 `examples/MinimalUsbSmoke`。
- **Phase B**：多组 `boards.txt` 菜单组合（`0x1000` / `0x26000` / `0x27000`、`usbcdc`、`usbdesc`、`buildprofile`），并用 `arm-none-eabi-objdump` 校验 **`.isr_vector` VMA** 与预期应用起始地址一致。
- **Phase D（部分）**：`usbdesc=no_app_dfu` 对应编译开关 `NRF_USB_RUNTIME_DISABLE_APP_DFU`，验证裁剪运行时 DFU 接口后仍可链接。
- **Phase A（参考镜像）**：可选浅克隆 `pdcook/nRFMicro-Arduino-Core`、`platformio/platform-nordicnrf52` 至 `.external/`。**不在此克隆** `ICantMakeThings/Nicenano-NRF52-Supermini-PlatformIO-Support`（上游含 Windows 非法路径字符 `|`）。

运行示例：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/usb_single_cable_ci.ps1
# 跳过 clone：
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/usb_single_cable_ci.ps1 -SkipClone
```

克隆板若串口 DFU **传输已完成** 但脚本长时间卡在 `Checking whether the board left bootloader mode`，多半是 Windows 未稳定识别应用 PID `0x00B4`。可在 **PowerShell** 会话中设置  
`$env:NIUS_SKIP_POST_VERIFY='1'`  
再执行 `arduino-cli upload`，则 **`upload.ps1` 在写完固件后跳过 PnP 校验**（仓库 `tools/niusrobotlab/upload.ps1` 已支持）。**不要**在外层再套一层 `powershell -Command "$env:..."`，否则 `$env` 会被提前展开丢失。

**免按键用户态 COM 烧录（ProMicro `239A:00B3` 克隆）**：Arduino CLI 自带的 1200 touch 会与 PowerShell **抢 COM**。已将 **`promicro_nrf52840` 侧 `upload.use_1200bps_touch=false`**，并在 **`platform.txt`** 的 `niusdfu.upload.pattern.windows` 中 **固定传入 `-UseTouch1200 'true'`** 给 **`upload.ps1`**，由脚本执行 **115200 → 1200** 两阶段 touch（含 DTR 边沿）、**touch 前终止残留 nrfutil**，且仅在设置 **`NIUS_ALLOW_SKIP_TOUCH_IF_BOOTLOADER_PORT=1`** 时才跳过 touch（默认同 PID 克隆板 **总是 touch**，避免二次上传误判）。全自动：`scripts/auto_upload_promicro_minimal.ps1 -UseArduinoCiConfig -MinimalUsb`；COM 独占排查：`scripts/test_serial_quick.ps1 COMx`。

一键脚本（仓库根目录，先进 bootloader / 端口出现后）：  
`powershell -NoProfile -ExecutionPolicy Bypass -File scripts/hardware_upload_minimal_usb.ps1 -Port COM3 -UseArduinoCiConfig`

当前实测克隆布局：`-BootloaderMenu promicroserialnosd`（默认，**0x1000，无 SoftDevice serial DFU**）。该菜单启用 `NRF_USBD_POLL_ONLY=1`，运行态服务口仍可能保持 `0x239A:0x00B3`，因此用户态与 bootloader **同 PID**，不能只靠 VID/PID 判阶段。

**当前结论：V1（无按键同口二次烧录）已闭环。** 用户态 `COM3` 二次自动上传可工作：1200 bps touch 触发固件 `NVIC_SystemReset` 进入 bootloader，adafruit-nrfutil 完成 DFU，板子回到用户态保留 `COM3`。原先 `Sending DFU start packet` 卡死的根因（EP0 OUT EasyDMA 未触发 / EP0 DATADONE 用 `BMREQUESTTYPE` 误判方向 / 后续 DTR=true 在 40 ms 确认窗口内取消 touch）已在固件侧修复，详见 [`platform/USB_1200_TOUCH_V1_FIX.md`](platform/USB_1200_TOUCH_V1_FIX.md)。用户 CDC 第二路（V2）与 USB GDB 仍待真机闭环验证。

## Phase 完成度总览

| Phase | 自动化结论 | 仍需真机 |
|-------|------------|----------|
| A 证据采集 | 参考仓库已脚本化拉取（除外 Nicenano PIO 仓库）；设备 VID/PID/COM 仍需你在 Windows 上填表 | 是 |
| B 布局与镜像 | CI 已验证 `.isr_vector` 与 `bootloader_app_start` 一致（含 0x26000 / 0x27000） | 否（枚举仍要真机确认） |
| C 最小 USB | CI + 真机：`MinimalUsbSmoke` 可枚举出 CDC COM（EP0 STATUS / 分包修复后） | 用串口监视器确认 `setup OK` / `tick` |
| D 复合设备收窄 | reset-cause reporter、detach 魔数、EP0 控制传输与 OS 描述符分包 | 双 CDC、裁剪 DFU 对照实验 |
| E 产品约定 | `Serial` 绑定维护口已落地；本板首次上传后最小 CDC 枚举已恢复 | **用户态同口二次上传 touch**、双 CDC、长时间稳定性 |
| F 风险 | 文档保留 | — |

## 目标

- **烧录**：依赖 bootloader + 主机工具（如 adafruit-nrfutil / 平台上传脚本），通过 **1200bps touch** 等方式进入 bootloader，无需按键。
- **串口**：在 **USB CDC 菜单关闭** 时仍能通过 **`Serial` 使用维护口（第一路 CDC）** 输出日志（见代码变更）；上传工具同样使用该端口触发复位。
- **可选用户 CDC**：菜单开启时保留第二路 CDC 供用户代码使用。
- **调试**：目标是 USB GDB stub 走服务 CDC；目前未完成真机验证，SWD 仍是最终兜底手段（本文仍假设无 SWD）。

## 现象摘要（记录基线）

| 现象 | 可能含义 |
|------|-----------|
| 刷机后仅有 COM1 | COM1 常为 PC 主板串口；若小板未枚举出新 CDC，说明应用侧 USB 未起来、未跑到初始化，或 bootloader 写入/跳转的 application start 与链接地址不一致。 |
| Bootloader 托盘可见 nice!nano、无盘符 | 克隆栈常见为串口 DFU + 可选 UF2；**无 MSC 盘符不一定异常**。 |
| 上传成功但应用无 USB | 多为 **Flash 布局/向量表与 bootloader 不一致**（如 0x26000 vs 0x27000），或上电即 Fault。 |

## Phase A — 证据采集（无 SWD）

- [ ] （真机）换机或用 Linux `lsusb` / Windows 设备管理器记录 **bootloader / 应用** 下的 VID:PID 与接口描述。
- [ ] （真机）Bootloader 连接时确认是否出现 **USB Serial Device（COMx）**。
- [ ] （真机）记录当前 IDE/`boards.txt` 选项：**Bootloader（auto / 0x26000 / 0x27000）**、**USB CDC**、**Build profile**。
- [x] （真机）`MinimalUsbSmoke` 在 EP0 修复后可持续读出 **`tick`**（维护口 CDC）；VID/PID 仍可能与 bootloader 同为 **239A:00B3**，不宜单靠 PID 区分阶段。
- [x] （CI）参考源码镜像：`scripts/usb_single_cable_ci.ps1` 可选克隆至 `.external/`（见上文排除说明）。

## Phase B — 布局与镜像（优先于大规模重构）

- [x] （CI）分别用 **app start 0x1000**、**0x26000** 与 **0x27000** 编译 `MinimalUsbSmoke`，**`.isr_vector` VMA** 与菜单一致（见 `build/usb_ci/REPORT.md`）。
- [ ] （真机）对照 USB 枚举结果（刷两种布局之一，确认能否稳定进应用）。
- [x] （真机）`EarlyBootReturnSmoke` 诊断确认当前克隆 **真实 application start 为 0x1000**：`0x26000/0x27000` 均不会执行到回 DFU，`0x1000` 能自动回到 `239A:00B3` COM3。
- [x] （真机）`promicroserialnosd + MinimalUsbSmoke + usbcdc=disabled + usbdesc=no_app_dfu` 已验证：上传链路可用；EP0 STATUS + 分包修复后可枚举 `239A:00B3` 复合设备 + CDC COM。
- [ ] （人工）核对 **SoftDevice / linker**（`nrf52840_s140_compat.ld`）与克隆 bootloader 是否同属 S140 家族（CI 未代替 datasheet 核对）。当前板更像 **无 SoftDevice serial DFU 布局**。

**结论位**：本机实测已定位为 **0x1000 app start**，并且 preinit/setup 诊断证明 bootloader 可以跳进应用。reset-cause + detach 魔数解决误回 DFU；**EP0 控制传输缺失 STATUS / 超长 GET_DESCRIPTOR 分包**曾导致 Windows「描述符请求失败」，已在 `NrfUsbd` 修复。下一步主要是串口日志确认与双 CDC / GDB 等产品路径验证。

## Phase C — 最小 USB 验证固件

- [x] （CI）`examples/MinimalUsbSmoke` 已在矩阵中编译（含 `usbcdc=disabled`、`buildprofile=debug`）。
- [x] （真机）`MinimalUsbSmoke`：`Serial.begin` + `tick`（DTR 置位）；进一步可做 **双 CDC** / **`usbgdbstub`** 回归。
- [x] （真机/无 SWD）`examples/ResetCauseReporter` 通过 `0x20004004` 保留 SRAM cause + 延迟编码完成 reset-cause 读取。

## Phase D — USB 复合设备收窄实验（按需）

仅在 Phase B/C 仍失败时进行分支实验：

- [x] （代码+CI）板菜单 **USB runtime DFU interface → Experimental: omit application DFU interface**（`-DNRF_USB_RUNTIME_DISABLE_APP_DFU=1`）；CI 矩阵含 `usbdesc=no_app_dfu`。
- [ ] （真机）对比 Windows 枚举是否与 composite+DFU 行为一致。
- [ ] （真机）**单 CDC**（`usbcdc=disabled`）与 **双 CDC** 的 COM 数量与稳定性。EP0 枚举修复后需回归复合设备行为。
- [x] （代码）`NrfUsbdDriver::begin()` 增加 READY/VBUS 容错、poll-only 模式、配置超时恢复与 CDC/USBD 阶段诊断。
- [x] （真机）增加持久 reset-cause 诊断，区分“bootloader 未激活应用”“应用主动 DFU 复位”“HardFault/Default_Handler/WDT 复位”；当前结论为无来源 detach 状态触发。

## Phase E — 产品化约定（单线无按键）

| 能力 | 实现要点 |
|------|-----------|
| 进 bootloader | 1200bps touch 作用于 **维护口**（第一路 CDC）。 |
| CDC Disabled | USB 栈仍可仅暴露维护口；**`Serial` 绑定维护口**（本仓库已实施）。 |
| CDC Enabled | `Serial` → 用户 CDC；上传触发仍可用维护口（复合设备上主机选对 COM）。 |

## Phase F — 风险与依赖

- **无 SWD**：迭代成本高；建议至少购置低成本 CMSIS-DAP。
- **Bootloader 损坏**：单 USB 无法恢复，必须 SWD 或返厂。
- **UF2 无盘符**：只要串口 DFU + touch 可用即可接受。

## 实施记录（维护）

| 日期 | 变更摘要 |
|------|-----------|
| 初次 | 本文档；`Serial` CDC Disabled→维护口；`MinimalUsbSmoke`；ProMicro 菜单 **omit application DFU**；`scripts/usb_single_cable_ci.ps1`；`platform.txt` 默认 `build.usbdesc_flags`。 |
| 2026-05-13 | 真机诊断确认当前 `239A:00B3` 克隆的应用起始地址为 **0x1000**；新增 `bootloader=promicroserialnosd`、`BootReturnSmoke` / `EarlyBootReturnSmoke` 诊断与硬件上传脚本。 |
| 2026-05-13 | `promicroserialnosd` 改为 poll-only USB、运行态 PID `0x00B3`，并增加未配置自动回串口 DFU；真机确认失败时可自动恢复 COM3。 |
| 2026-05-14 | 修复控制传输 EP0 IN：`EVENTS_EP0DATADONE` 且 SETUP 方向为 IN 时在完成数据阶段（含 ZLP）后触发 `TASKS_EP0STATUS`；EP0 IN 按 64 字节分包并用 `memmove` 处理 `controlInBuffer_` 重叠源；清除无效 detach 时对 SRAM cause 的误写。真机上传后不再出现 `VID_0000` 描述符请求失败，COM 口恢复。 |
| 2026-05-14 | 文档：`USB_SINGLE_CABLE_PLAN.md` 增补「关键要点与易踩坑」（bootloader AppStart、VID/PID、GPREGRET、EP0 STATUS、detach 魔数、SRAM、`NIUS_SKIP_POST_VERIFY`）；新增 [`DUAL_CDC_AND_USB_DEBUG_ROADMAP.md`](DUAL_CDC_AND_USB_DEBUG_ROADMAP.md)（双 CDC + GDB stub + IDE 任务清单）。 |
| 2026-05-18 | 当前真机状态更新：**首次上传 -> 用户态恢复 -> COM3 保留** 已成立；但 **用户态 COM3 二次自动上传** 仍失败。host 侧 1200 touch 后未观察到真实 USB detach，`adafruit-nrfutil` 在 `Sending DFU start packet` 卡住。 |
| 2026-05-19 | **V1 修复并通过**：固件侧三处协作 bug 均已修复（EP0 OUT EasyDMA 缺失 `TASKS_STARTEPOUT[0]`、EP0 DATADONE 用挥发的 `BMREQUESTTYPE` 误判 IN/OUT、确认窗口内 DTR=true 误清 `serviceTouchPending_`）；`scripts/verify_promicro_usbcdc_upload_behavior.ps1 -Phase V1` Pass A + Pass B 均退出 0。原有的服务口 boot-token 兜底机制（`~NIUSBL!42\r` + 134/8/2/2 line coding 武装）已从固件与 `upload.ps1` 中删除。详见 [`platform/USB_1200_TOUCH_V1_FIX.md`](platform/USB_1200_TOUCH_V1_FIX.md)。 |

**验证**：使用 `scripts/usb_single_cable_ci.ps1`（junction 本仓库 core）或 Sketchbook `hardware/` 指向本仓库后再编译。

## 是否需要重构整个代码库？

**默认不需要。** 先完成 Phase B 排除链接/向量表错误；若极简固件能在单维护口枚举成功，再按需收敛 `NrfUsbd` 描述符。仅当产品决策迁移至全新 SDK（如统一 Zephyr）时再评估平台级重写。
