# Release Notes — v0.1.5

Polish + UX iteration on top of [v0.1.4](RELEASE_NOTES_v0.1.4.md). Same
install URL:

```
https://raw.githubusercontent.com/dunknowcoding/ArduinoNRF/main/package_arduinonrf_index.json
```

## What changed

### Three key examples now appear in IDE 2's Examples menu

`hardware/arduinonrf/nrf52/libraries/Bootloader/examples/` now contains
the four sketches users actually need to find during normal use:

- `BootloaderRequest` — sketch-side API for triggering a software
  bootloader reset.
- `MinimalUsbSmoke` — single-COM smoke test that the V1 button-less
  reflash workflow targets.
- `UsbGdbStubBreakpoint` — halts at `bkpt #0` in `setup()` ready for a
  GDB attach via the `buildprofile=usbgdbstub` IDE 2 Debug button.
- `UsbGdbStubFault` — triggers a HardFault to demonstrate the GDB stub's
  fault-interception path.

In Arduino IDE 2 they live at **File → Examples → For AliExpress
ProMicro nRF52840 → Bootloader → …** — the top-level `examples/` folder
at the repo root is still kept for command-line invocation by the
verification scripts, but the IDE 2 menu only surfaces examples that
live inside a library's `examples/` directory, which is what this
release adds.

### Each upload-log line no longer prints twice

`upload.ps1` mirrors every progress line to stderr so plain terminals
get line-buffered live progress instead of waiting on stdout's
block-buffer to flush. Arduino IDE 2 captures BOTH stdout and stderr
from the arduino-cli child and prints them into the same Output panel,
so under IDE 2's verbose upload that mirror rendered every line twice
(the duplicate output reported by users). The mirror is now skipped
when `ArduinoIdeVerboseUpload=true` (the flag IDE 2 passes through
`platform.txt` when verbose upload is enabled). Plain-terminal callers
without that flag still get the live stderr-mirrored progress.

Override: `NIUS_DISABLE_UPLOAD_STDERR_MIRROR=1` to force off (already
existed); no new env var introduced.

### Compact branded banner + block-character progress bar

The upload header is now a 3-line Unicode box-drawn banner that
identifies the run cleanly:

```
┌────────────────────────────────────────────────────────────────┐
│  NiusRobotLab  ·  nRF52 Upload Console  ·  Target: promicro_nrf52840  │
└────────────────────────────────────────────────────────────────┘
```

The progress bar uses solid/light block characters:

```
▶ ████████████░░░░░░░░░░░░  50%  PUSH  Streaming firmware over Adafruit DFU on COM3
```

`upload.ps1` now forces its own stdout / stderr to UTF-8 (PowerShell
5.1 defaults to the active console codepage, which on Chinese Windows
is usually GBK — without the override the box-drawing and block chars
would arrive as mojibake when the IDE captures them).

No firmware changes; pure host-side tooling polish.
