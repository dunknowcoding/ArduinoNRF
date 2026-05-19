# 双 CDC + USB GDB Stub — 设计要点与任务清单

本文档与 `USB_SINGLE_CABLE_PLAN.md`（单线排障与已验证结论）配套：**前者记录「已经踩过的坑」**，本文记录 **产品向架构约定** 与 **未完成工作**，避免设计与实现脱节。

---

## 0. 协作约定（增量开发 + 真机）

自动化代理 **无法连接你的 USB 开发板**。约定：**每一逻辑步骤在继续扩展前，须由你在硬件上完成对应验收**，并按 `docs/platform/DEVBOARD_INCREMENTAL_GATES.md` 中的 **闸门 G0→G4** 逐级签字（简讯即可：闸门编号、通过/失败、COM 与日志摘要）。**未声明通过的闸门，不得视为已在真机上验证。**

---

## 1. 目标架构（共识）

| 接口 | 角色 | 主机期望 |
|------|------|-----------|
| **CDC #1（维护口 / Service）** | 串口 DFU 触发（1200bps touch）、系统日志、`usb_gdbstub_bridge` 对接 GDB stub | 固定枚举字符串便于识别（如现有「Nius Service …」） |
| **CDC #2（用户口 / User）** | 仅用户 Sketch `Serial` I/O | 启用 **`usbcdc=enabled`** 时出现 |

**原则**：GDB Remote Protocol **只走维护口**；用户 `print` **不占用 GDB 字节流**。

---

## 2. 与现有平台的衔接（已实现片段）

| 组件 | 路径 | 说明 |
|------|------|------|
| Stub 编译配置 | `boards.txt` → `buildprofile=USB CDC GDB stub` | `-DNRF_SYSTEM_USB_GDB_STUB=1`、`-DNRF_BUILD_PROFILE=2` |
| IDE 调试元数据 | `boards.txt` + `platform.txt` → `debug.usbgdbstub.*` | `gdbTarget=localhost:3335`、Cortex-Debug `postAttachCommands` 规避 OpenOCD `monitor` |
| 主机桥 | `tools/niusrobotlab/usb_gdbstub_bridge.ps1` | 串口 ↔ TCP，供 `arm-none-eabi-gdb` `extended-remote` |
| 运行时 stub | `NrfGdbStub.cpp` / `NrfGdbStub.h` | 需在 **维护 CDC** 上闭环真机验证 |

详见历史盘点：`docs/platform/USB_CDC_GDB_STUB_FILES.txt`（可能略早于 EP0/双 CDC 结论，以 `USB_SINGLE_CABLE_PLAN.md` 为准）。

---

## 3. Arduino IDE 2.x 集成要点（设计层）

- **依赖**：官方调试链路使用 **Cortex-Debug** + 平台 `recipe`，已指向 **external server + localhost:3335**。
- **双 CDC 风险**：IDE 的 `{serial.port}` 必须指向 **维护口 COM**；若默认连到用户口，桥接会失败。
- **可选增强**（任务）：
  - 文档强制说明「调试端口 = Service CDC」；见 `docs/platform/ARDUINO_IDE2_USB_GDBSTUB.md`。
  - `usb_gdbstub_bridge.ps1`：`-PreferServiceCdc`、`-PreferMiIndex`、`-MatchFriendlyName`，以及 **PID `0x00B3`/`0x00B4` 回退**（与 `upload.ps1` 共用 `Resolve-AdafruitSerialControlPortWithBoardIdentity`）。

**一般不强制**自研 Arduino Marketplace 插件；优先 **platform.txt + 文档 + 端口发现**。

---

## 4. VS Code 集成要点（设计层）

- **扩展**：推荐 **Cortex-Debug**（`marus25.cortex-debug`）或 **cppdbg**，不要求自定义语言扩展。
- **工作流**：
  1. `arduino-cli compile`（或 Arduino 扩展）生成带 `-g3` 的 ELF；
  2. `tasks.json`：`preLaunchTask` 启动 `usb_gdbstub_bridge.ps1 -SerialPort <维护口> -TcpPort 3335`；
  3. `launch.json`：`gdbPath` → 套件内 `arm-none-eabi-gdb`，`executable` → `.ino.elf`，目标 `localhost:3335`，附加命令与 `boards.txt` 中 stub 配置对齐。
- **交付物**（任务）：仓库内提供 **`docs/examples/vscode/`**（`tasks.json`、`launch.json`）；Arduino IDE 2 步骤见 **`docs/platform/ARDUINO_IDE2_USB_GDBSTUB.md`**。

---

## 5. 任务清单（TODO）

### 5.1 固件 / USB 栈

- [ ] **`usbcdc=enabled` 下长时间稳定性**：双 CDC 枚举、Windows/Linux 各一轮。
- [ ] **确认 GDB stub 仅绑定维护口**：用户口打开串口监视器不会打断 GDB 会话。（代码层面：`NrfGdbStub` 注释已标明维护 CDC；仍需硬件闭环。）
- [ ] **Stub 与 `NRF_USBD_POLL_ONLY` 共存验证**：避免 IRQ/poll 竞争导致 stub 丢包。
- [ ] **可选**：维护口字符串 / IAD 顺序文档化，便于脚本识别。

### 5.2 主机工具链

- [x] **桥接脚本**：支持 `-MatchFriendlyName`、`-PreferMiIndex`、`-PreferServiceCdc`（Windows `Get-PnpDevice`），以及 **`promicro_nrf52840` 运行时 PID `0x00B3` + `0x00B4` 回退**（与上传 remap 共用 `Resolve-AdafruitSerialControlPortWithBoardIdentity`）。
- [ ] **与 `arduino-cli board list` 输出对齐**：上传脚本与调试脚本共用同一套端口解析（可选）。

### 5.3 文档与示例

- [ ] **`USB_SINGLE_CABLE_PLAN.md`**：持续同步「易踩坑」（单一事实来源的子集摘要已合并进该文档章节）。
- [x] **VS Code**：`docs/examples/vscode/tasks.json`、`launch.json`（桥接后台任务 + Cortex-Debug external）。
- [x] **Arduino IDE 2**：`docs/platform/ARDUINO_IDE2_USB_GDBSTUB.md`（选板、profile、维护口 COM、先桥接后调试）。

### 5.4 CI（无硬件）

- [x] 在现有矩阵中增加 **`bootloader=promicroserialnosd` + `usbcdc=enabled` + `buildprofile=usbgdbstub`** 编译组合（`scripts/usb_single_cable_ci.ps1`）。
- [ ] （可选）脚本校验 `boards.txt` / `platform.txt` 中 stub 调试键完整性。

---

## 6. 非目标（当前迭代刻意不做）

- 替代 OpenOCD 的 SWD 文案删除（SWD 仍为兜底与量产修复手段）。
- Arduino IDE 内「烧录后自动展开调试面板」的全自动 UX（需 IDE 插件级能力，单独立项）。
- 在 **单 CDC** 上复用同一 COM 混跑 GDB + 用户 printf（避免作为默认产品设计）。

---

## 7. 参考链接（仓库内）

- `docs/USB_SINGLE_CABLE_PLAN.md` — 单线、bootloader、EP0、reset-cause、上传脚本
- `docs/platform/USB_CDC_GDB_STUB_FILES.txt` — stub 相关文件盘点
- `docs/boards/promicro_nrf52840.md` — 板级证据与 bootloader 摘要
- `hardware/arduinonrf/nrf52/platform.txt` — `debug.usbgdbstub.*`
- `docs/platform/ARDUINO_IDE2_USB_GDBSTUB.md` — Arduino IDE 2 + USB GDB stub 一页流程
