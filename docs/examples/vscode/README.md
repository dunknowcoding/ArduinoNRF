# VS Code debug examples

This directory contains minimal sample configuration files for attaching VS Code's `cortex-debug` extension to the ArduinoNRF USB CDC GDB stub bridge.

## Files

- `launch.json` - attaches `arm-none-eabi-gdb` to `localhost:3335` and expects an ELF at `build/Debug/sketch.ino.elf`.
- `tasks.json` - starts `usb_gdbstub_bridge.ps1` for `promicro_nrf52840` on TCP port `3335` and waits until the bridge is ready for a GDB client.

## Assumptions

- The board is running a `usbgdbstub` build such as `examples/UsbGdbStubBreakpoint`.
- The service / maintenance CDC is the port used for the bridge.
- The example is ProMicro-oriented; adjust `-Board`, ELF path, or TCP port as needed for your workspace.

For the end-to-end Arduino IDE 2 flow and bridge behavior, see [../../platform/ARDUINO_IDE2_USB_GDBSTUB.md](../../platform/ARDUINO_IDE2_USB_GDBSTUB.md).
