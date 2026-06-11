# Arduino IDE 2: USB CDC GDB stub debugging (single cable)

Debug a sketch over the **same USB cable used for upload** — no J-Link, no
SWD wiring. The stub lives inside the firmware (DebugMonitor exception, not
halt-mode), a host bridge turns the maintenance CDC into a GDB server on
`localhost:3335`, and Arduino IDE 2 / VS Code `cortex-debug` attach to it.

Hardware-verified end to end on the ProMicro nRF52840: breakpoints,
single-step (`stepi`), **Pause** of free-running code, **DWT watchpoints**
(data breakpoints), peripheral/memory reads while stopped, and restart.
With dual CDC enabled, the user `Serial` port stays usable concurrently on
the second COM port.

## 1. Board and build options

1. Board: e.g. **ProMicro nRF52840** (`promicro_nrf52840`).
2. **Build profile → USB CDC GDB stub** (defines `NRF_SYSTEM_USB_GDB_STUB`,
   builds with `-g3`).
3. For dual CDC, set **USB CDC → Enabled**: user `Serial` gets its own COM
   port; GDB traffic stays on the maintenance port.

## 2. Pick the right serial port (the maintenance CDC)

The port selected in the IDE must be the **maintenance / service CDC**
(USB interface `MI_00`), not the user `Serial` port (`MI_02`). On Windows,
check Device Manager friendly names, or let the bridge choose:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\hardware\arduinonrf\nrf52\tools\niusrobotlab\usb_gdbstub_bridge.ps1 `
  -Board promicro_nrf52840 -PreferServiceCdc -MatchFriendlyName "Service"
```

With no `-SerialPort` and a single target board attached, the bridge
auto-selects by VID/PID (including the `0x00B3` / `0x00B4` fallbacks) and
interface preference. `-ListPorts` prints the candidates.

## 3. Start order (IDE 2 + external GDB server)

The platform configures Cortex-Debug as an **external server**: GDB connects
to **`localhost:3335`**, and the IDE does *not* spawn the bridge itself.

1. Start the bridge first (terminal, or a VS Code task per
   [docs/examples/vscode/tasks.json](../examples/vscode/tasks.json)):
   `usb_gdbstub_bridge.ps1 -Board promicro_nrf52840 -SerialPort COMx -TcpPort 3335`
   (add `-PreferServiceCdc`, and `-MatchFriendlyName` if needed, for dual CDC).
2. Wait for **`Waiting for a GDB client connection...`**.
3. Click **Debug** in Arduino IDE 2 (it attaches to the running stub).

On Linux/macOS the bridge is `usb_gdbstub_bridge.py` with the equivalent
`--serial-port / --tcp-port / --board` arguments (see `platform.txt`,
`debug.usbgdbstub.bridge.launch.pattern.*`).

## 4. Flashing while debugging

Re-flash through the normal upload path (`arduino-cli upload` / IDE upload —
serial DFU). Do **not** try to program flash through the stub or attach a
SWD probe while the DebugMonitor stub is active — an external halt-mode
debugger and the monitor stub fight over the debug authority, and stale
break bytes in the host serial driver can surface as phantom stop replies on
the next session.

## 5. Known behavior and limits

- The stub uses the **DebugMonitor** exception: code in fault/ISR context at
  higher priority than the monitor cannot be stepped (the stub masks
  interrupts via `BASEPRI` during `stepi` so stepping stays on the user's
  instruction stream).
- **Pause** works by pending the monitor exception on the break byte
  (`0x03`); watchpoints use the DWT comparators.
- One GDB client at a time; the bridge owns the maintenance CDC while
  running, so close it before a serial-DFU upload on the same port (the
  bridge yields automatically for the platform's own upload tool).

## 6. References

- `hardware/arduinonrf/nrf52/platform.txt` — `debug.usbgdbstub.*`,
  `bridge.launch.pattern`
- `hardware/arduinonrf/nrf52/boards.txt` — `debug.cortex-debug.custom.*`
  under the `usbgdbstub` build profile
- [docs/examples/vscode/](../examples/vscode/) — VS Code `tasks.json` /
  `launch.json` templates
- `examples/UsbGdbStubBreakpoint` — a sketch made to be debugged
