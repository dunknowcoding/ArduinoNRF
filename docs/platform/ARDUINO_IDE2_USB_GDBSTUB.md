# Arduino IDE 2：USB CDC GDB stub 调试（单线）

本文描述在本平台使用 **Build profile → USB CDC GDB stub** 时的推荐顺序；与 `docs/VALIDATION.md` 中的「维护口 / 用户口」分工一致。

## 1. 板卡与编译选项

1. 板型：例如 **ProMicro nRF52840**（或当前使用的 `promicro_nrf52840` 变体）。
2. **Build profile**：选 **USB CDC GDB stub**（定义 `NRF_SYSTEM_USB_GDB_STUB`，`-g3`）。
3. 若使用双 CDC：**USB CDC** 菜单选 **Enabled**（用户 `Serial` 走用户口；GDB 仍只走维护口）。

## 2. 串口（必须是维护口）

在 IDE 右下角或工具栏选择的 **串口必须对应 Service / 维护 CDC**。若误选用户口串口监视器用的 COM，`debug.usbgdbstub` 桥接会连错接口。

双 CDC 时在 Windows 上可对照设备管理器友好名称；也可用主机脚本缩小范围，例如：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\hardware\arduinonrf\nrf52\tools\niusrobotlab\usb_gdbstub_bridge.ps1 `
  -Board promicro_nrf52840 -PreferServiceCdc -MatchFriendlyName "Service"
```

未指定 `-SerialPort` 且仅有一块目标板时，脚本可按 VID/PID（含 `0x00B3` / `0x00B4` 回退）与 MI 偏好尝试自动选口。

## 3. 启动顺序（IDE 2 + external GDB）

平台将 Cortex-Debug 配成 **external server**，GDB 连接 **`localhost:3335`**，并不自动拉起桥接进程。

1. **先**在终端运行桥接（或 IDE **Run Task** 若你已按 `docs/examples/vscode/tasks.json` 配好同类任务）：
   - `usb_gdbstub_bridge.ps1 -Board promicro_nrf52840 -SerialPort COMx -TcpPort 3335`
   - 双 CDC 建议加 `-PreferServiceCdc`，必要时 `-MatchFriendlyName`。
2. 确认桥接打印 **Waiting for a GDB client connection...**
3. 再在 Arduino IDE 2 中点击 **调试**（附加到 stub）。

## 4. 与 arduino-cli / 本地脚本并行

若你用 **`arduino-cli`** 编译上传同一套菜单选项，`hardware_upload_minimal_usb.ps1 -UseArduinoCiConfig …` 与 CI 使用相同的 **`--config-file .arduino-ci.yaml`** + sketchbook junction。IDE 里选的串口应与 **`arduino-cli … board list`** 以及 **`usb_gdbstub_bridge.ps1`** 使用的维护口一致。

## 5. 参考

- `hardware/arduinonrf/nrf52/platform.txt` — `debug.usbgdbstub.*`、`bridge.launch.pattern`
- `hardware/arduinonrf/nrf52/boards.txt` — `usbgdbstub` 下的 `debug.cortex-debug.custom.*`
- `docs/examples/vscode/` — VS Code `tasks.json` / `launch.json` 模板
