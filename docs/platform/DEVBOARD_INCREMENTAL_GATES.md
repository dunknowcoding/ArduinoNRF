# 开发板增量验收闸门（双 CDC / GDB stub / 主机脚本）

Cursor Agent **无法接入你的 USB/COM 硬件**，因此「每一步改完先在板上测到通过」只能由你本地执行；Agent 侧负责：拆小步、给出 **FQBN / 命令 / 期望现象**，并在你回报「闸门 Gx 通过」后再写下一步代码。

下列闸门与 `docs/VALIDATION.md`、`docs/platform/UPLOAD_BEHAVIOR.md` 对齐；**按序号做**，只有通过才进入下一闸门。

---

## arduino-cli 与本地工具（统一入口）

**前置**：已安装 `arduino-cli` 并能执行 `arduino-cli version`；已安装 **ARM GCC 工具链**（通常随 Arduino nRF52 / Mbed core 或手动安装）。

### A. 使用「本仓库」硬件（推荐：与 CI 一致）

适用于：**没有把本仓库拷进 Arduino15 sketchbook**、只在克隆目录里开发。

1. 在仓库根目录打开 PowerShell。
2. 上传脚本使用 **`-UseArduinoCiConfig`**：会自动维护 `.arduino-ci-sketchbook` → `hardware/arduinonrf/nrf52` 目录联接，并对所有 `arduino-cli` 调用传入 **`--config-file .arduino-ci.yaml`**（逻辑见 `scripts/arduino_cli_with_repo.ps1`，与 `scripts/usb_single_cable_ci.ps1` 同源）。
3. 一键编译上传（内部含 `board list`）：

   ```powershell
   .\scripts\hardware_upload_minimal_usb.ps1 -Port COMx -BootloaderMenu promicroserialnosd -UseArduinoCiConfig -MinimalUsb
   ```

   双 CDC + GDB stub（闸门 G3）：

   ```powershell
   .\scripts\hardware_upload_minimal_usb.ps1 -Port COMx -BootloaderMenu promicroserialnosd -UseArduinoCiConfig -DualCdcUsbGdbStub
   ```

### B. 手写 arduino-cli（调试用）

设仓库根为当前目录，草图为 `examples\MinimalUsbSmoke`，与 CI **`dual_cdc_usbgdbstub_1000`** 一致的 FQBN：

```text
arduinonrf:nrf52:promicro_nrf52840:bootloader=promicroserialnosd,usbcdc=enabled,buildprofile=usbgdbstub
```

```powershell
arduino-cli --config-file .arduino-ci.yaml compile --fqbn "arduinonrf:nrf52:promicro_nrf52840:bootloader=promicroserialnosd,usbcdc=enabled,buildprofile=usbgdbstub" --clean examples\MinimalUsbSmoke
arduino-cli --config-file .arduino-ci.yaml upload -p COMx --fqbn "arduinonrf:nrf52:promicro_nrf52840:bootloader=promicroserialnosd,usbcdc=enabled,buildprofile=usbgdbstub" examples\MinimalUsbSmoke
```

若尚未建立 sketchbook 联接，可先跑一次带 `-UseArduinoCiConfig` 的上传脚本，或在 PowerShell 中：

```powershell
. .\scripts\arduino_cli_with_repo.ps1
Get-ArduinoCliRepoConfigArgs -EnsureJunction | Out-Null
```

枚举端口：

```powershell
arduino-cli --config-file .arduino-ci.yaml board list
```

Python（conda 环境 **`IronEngineWorld`**）：列出 COM、`PNPDeviceID` / `InstanceId`（含 **`MI_xx`**）。推荐在该环境中安装 **`pyserial`**（`conda run -n IronEngineWorld pip install pyserial`）以获得 **VID/PID** 与序列号等摘要；未安装时仍可通过 Win32/PnP 路径列出设备。

```powershell
conda run -n IronEngineWorld python scripts\win_serial_inventory.py
conda run -n IronEngineWorld python scripts\win_serial_inventory.py --json
```

**一键软件自检（不含上传 / DFU）**：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify_platform_local.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify_platform_local.ps1 -SkipCi -WithIronEngineInventory
```

上传仍用 `hardware_upload_minimal_usb.ps1`。若 DFU 长时间卡在约 **90%**：先 **关闭占用 COM 的软件**、检查 USB；**`promicro_nrf52840` 串口 bootloader 菜单默认 `upload.use_1200bps_touch=false`**（见 `boards.txt`），并不走 IDE 常见的「1200 baud touch」流程。

### C. 主机桥接（仓库内脚本）

自仓库根目录：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\hardware\arduinonrf\nrf52\tools\niusrobotlab\usb_gdbstub_bridge.ps1 -Board promicro_nrf52840 -PreferServiceCdc -TcpPort 3335
```

指定 COM（与 `board list` 交叉核对）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\hardware\arduinonrf\nrf52\tools\niusrobotlab\usb_gdbstub_bridge.ps1 -Board promicro_nrf52840 -SerialPort COM3 -TcpPort 3335
```

---

## 步骤 H — 「主机端口与桥接」实机验收（ProMicro / 无按键）

**目的**：验证 **`usb_port_helpers` / `usb_gdbstub_bridge`** 在当前 Windows + 已插板条件下正确（PID `0x00B3`/`0x00B4`、`-PreferServiceCdc`、维护口 remap、`Describe`）。

**前置**：关闭占用开发板 COM 的程序（Arduino IDE 串口监视器、PuTTY、旧桥接等），否则串口独占会失败。

**命令（仓库根目录）**：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\hw_verify_step_host_bridge.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\hw_verify_step_host_bridge.ps1 -TcpSmoke -TcpPort 3337
```

**判定**：H1 打印两条 VID/PID 候选；H2 在无歧义时应选出维护口 COM（典型 **`MI_00`**）；H4 维护口为 **`selected port already on control/service interface`**；H5 JSON 合法；H6（`-TcpSmoke`）应 **`CONNECT OK`**。

**无物理按键**上传说明：本仓库 `promicro_nrf52840` 的 **`promicroserial` / `promicroserialnosd` / `promicroseriallegacy`** 菜单在 `boards.txt` 中均为 **`upload.use_1200bps_touch=false`**，上传依赖 **串口 DFU 数据路径**，不是手动按键进 bootloader。传输卡住时先释放 COM、检查 USB；Adafruit 生态另有 **double-tap reset** 的讨论（例如 [`Adafruit_nRF52_Bootloader#26`](https://github.com/adafruit/Adafruit_nRF52_Bootloader/issues/26)），取决于硬件是否能产生两次复位，克隆板未必等同。

---

## 闸门 G0 — 环境与启动地址基线

**目的**：确认当前板卡应用起始地址与 bootloader 菜单一致（克隆常见 `0x1000` + `promicroserialnosd`）。

**步骤**（任选已文档化的诊断草图，例如 `EarlyBootReturnSmoke` / BootReturn 系列），确认：

- 上传后应用可持续运行，不出现秒级掉回 DFU（除非刻意触发）。
- 记录维护口 COM（后续上传与 GDB 桥都用它）。

**通过标准**：你能在维护口看到预期日志或诊断结论，且枚举 VID/PID 与文档一致（常见运行态 `239A:00B3`）。

---

## 闸门 G1 — 最小 USB 冒烟（单 CDC / 与你日常选项一致）

**目的**：证明 USB 栈与本机构型仍正常。

**命令示例**：

```powershell
.\scripts\hardware_upload_minimal_usb.ps1 -Port COMx -BootloaderMenu promicroserialnosd -UseArduinoCiConfig -MinimalUsb
```

（将 `COMx` 换成 `arduino-cli --config-file .arduino-ci.yaml board list` 或 IDE 里 bootloader / 维护口对应端口。）

**通过标准**：设备管理器出现复合设备 + CDC；串口监视器在维护口可读到 `MinimalUsbSmoke` 的 `setup OK` / `tick`（行为以草图为准）。

---

## 闸门 G2 — 主机端口脚本（PID 回退 / MI / 友好名）

**目的**：验证 **`usb_port_helpers.ps1` / `usb_gdbstub_bridge.ps1`** 在你这台 Windows 上选对维护口（尤其双 CDC 时）。

**步骤**：

1. 列出端口：

   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File .\hardware\arduinonrf\nrf52\tools\niusrobotlab\usb_gdbstub_bridge.ps1 -ListPorts
   ```

2. 指定板型自动优选（双 CDC 建议加 `-PreferServiceCdc`，必要时 `-MatchFriendlyName`）：

   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File .\hardware\arduinonrf\nrf52\tools\niusrobotlab\usb_gdbstub_bridge.ps1 `
     -Board promicro_nrf52840 -PreferServiceCdc -TcpPort 3335
   ```

**通过标准**：桥接进程监听 `3335`，且打开的串口经设备管理器确认为 **Service / MI_00**（或你们定义的维护口），误连用户口时应能通过参数纠正。

---

## 闸门 G3 — 双 CDC + USB GDB stub 编译体（与 CI 矩阵一致）

**目的**：板上运行的固件与 `scripts/usb_single_cable_ci.ps1` 中的 **`dual_cdc_usbgdbstub_1000`** 组合一致。

**命令示例**：

```powershell
.\scripts\hardware_upload_minimal_usb.ps1 -Port COMx -BootloaderMenu promicroserialnosd -UseArduinoCiConfig -DualCdcUsbGdbStub
```

**通过标准**：

- Windows 下可见 **两路 CDC**（或两个 COM），其中一路用于用户 `Serial`、一路为维护口（与平台约定一致）。
- 维护口仍可完成串口 DFU 上传路径（本仓库 **promicro 串口 bootloader 菜单默认 `upload.use_1200bps_touch=false`**，不是 IDE「1200 触摸」依赖）。
- 长时间 **≥10 分钟** 无异常掉口、无静默复位（若失败请记日志与设备管理器截图）。

---

## 闸门 G4 — GDB 闭环（维护口）

**前提**：G3 已通过。

**步骤**：

1. 终端 A：启动桥（务必指向维护口；不确定时用 G2 的参数）：

   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File .\hardware\arduinonrf\nrf52\tools\niusrobotlab\usb_gdbstub_bridge.ps1 `
     -Board promicro_nrf52840 -PreferServiceCdc -TcpPort 3335
   ```

2. 终端 B：`arm-none-eabi-gdb`（或 IDE/Cortex-Debug）对 **`localhost:3335`** `extended-remote`，加载 **与板上 ELF 一致** 的符号文件。

**通过标准**：能附加、断点命中、`continue` 行为符合 stub 设计；**用户口开串口监视器不应独占 GDB 字节流**（若异常说明分流/regression，勿进入下一步）。

---

## 回报格式（给 Agent）

便于增量迭代，请复制填写：

```text
闸门：G_
结果：通过 / 失败
板卡 & 菜单：promicro_nrf52840; bootloader=…; usbcdc=…; buildprofile=…
COM：维护口=COM_ 用户口=COM_（若适用）
现象：（日志片段 / 设备管理器 MI / 是否掉 DFU）
```

Agent 在收到 **「Gx 通过」** 之前，不应假定该步已在硬件上成立。
