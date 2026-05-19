# Release Notes — v0.1.1

Iteration on top of [v0.1.0](RELEASE_NOTES_v0.1.0.md). Same install URL:

```
https://raw.githubusercontent.com/dunknowcoding/ArduinoNRF/main/package_arduinonrf_index.json
```

If you already installed `0.1.0`, open Arduino IDE 2.x's **Tools → Board →
Boards Manager**, search for "Arduino NRF52 Boards", and click **Update**
to pull `0.1.1`.

## Highlights

### One-click Debug for the USB-CDC GDB stub

`buildprofile=USB CDC GDB stub` now spawns the host-side bridge
automatically when you press the Debug button in Arduino IDE 2 — no
separate "start bridge" terminal needed.

How it works:

- `boards.txt` for `promicro_nrf52840` overrides `debug.server=openocd`
  (instead of the custom name `usbgdbstub`, which `arduino-cli` rejects
  as "GDB server not supported") AND overrides `debug.server.openocd.path`
  to point at a new wrapper
  [`hardware/arduinonrf/nrf52/tools/niusrobotlab/usb_gdbstub_server.cmd`](../../hardware/arduinonrf/nrf52/tools/niusrobotlab/usb_gdbstub_server.cmd).
- The wrapper ignores all command-line args it receives (`arduino-cli`
  hands it gdb-pipe-mode flags; cortex-debug hands it openocd-style
  flags — neither is meaningful for our TCP bridge) and launches
  [`usb_gdbstub_bridge.ps1`](../../hardware/arduinonrf/nrf52/tools/niusrobotlab/usb_gdbstub_bridge.ps1)
  with `-PreferServiceCdc` so the bridge auto-picks the SERVICE/MI_00
  CDC for the configured board.
- The bridge prints `Listening on port 3335 for gdb connections` right
  after its TCP listener is ready. That line matches cortex-debug's
  default openocd `serverReady` regex, so cortex-debug treats the bridge
  as a started server and then connects GDB to `localhost:3335` (the
  `gdbPort`/`gdbTarget` set by the same boards.txt menu overrides).
- The full set of monitor-style postAttachCommands has been cleared to
  empty so cortex-debug doesn't send openocd-only `monitor reset halt` /
  `monitor gdb_sync` commands to the stub.

### Known limitations

- **Coexistence with upload is still manual**: while a debug session is
  active, the bridge holds the SERVICE COM open. An `arduino-cli upload`
  (or the IDE Upload button) on the same COM will fail until you stop
  the debug session first. The bridge-yield-on-upload IPC mechanism
  described in the project plan lands in a follow-up release.
- **Only `promicro_nrf52840` has the auto-launch wiring** in this
  release. Other boards with `buildprofile=usbgdbstub` (nicenano_v2,
  supermini_nrf52840, etc.) still inherit the platform.txt defaults,
  which point at the real openocd. If you need debug on those boards,
  copy the
  `<board>.menu.buildprofile.usbgdbstub.debug.server.openocd.path*`
  overrides from the promicro_nrf52840 block in `boards.txt`.
- **Single FPB capacity**: the Cortex-M4 FPB unit on this MCU has 6
  hardware breakpoint comparators. Software breakpoints in RAM-resident
  code are not supported.

## How to use

1. Install or update via Boards Manager (`Tools → Board → Boards Manager`).
2. Select board `AliExpress ProMicro nRF52840`, menu option
   `Build profile → USB CDC GDB stub (-Og -g3)`.
3. Compile + upload `examples/UsbGdbStubBreakpoint`.
4. The board halts at the inline `bkpt #0` in `setup()`.
5. Click **Debug** in the IDE — the bridge spawns automatically,
   cortex-debug attaches, the Variables and Registers panes populate at
   the breakpoint. Step / Continue work via the normal IDE buttons.

See [`docs/platform/ARDUINO_IDE2_USB_GDBSTUB.md`](../platform/ARDUINO_IDE2_USB_GDBSTUB.md)
for the full workflow.
