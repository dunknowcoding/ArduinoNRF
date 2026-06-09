# Hardware validation status

What has been verified on real hardware, and how to reproduce it. Reference board: **AliExpress ProMicro nRF52840 clone** (nice!nano-compatible bootloader family `0x239A:0x00B3`, UF2 label `NICENANO`).

On this clone the runtime service CDC and the bootloader share the same VID:PID, so VID/PID alone cannot tell host tooling which mode the board is in; the upload pipeline (`upload.ps1`) copes with same-PID-aware detection.

## ✅ Verified

| Capability | Status | Notes |
|---|---|---|
| Hands-free reupload (single cable) | **PASS** | A second `arduino-cli upload` on the same COM needs no manual reset — the 1200-bps touch triggers `SYSRESETREQ` into the bootloader, adafruit-nrfutil streams the image, the board returns to user mode. |
| Single-cable USB-CDC debug (Arduino IDE 2) | **PASS** | Breakpoints (FPB), step in/over/out, registers, memory, data watchpoints (DWT), pause (DebugMonitor), peripheral/SVD reads — no external probe. |
| Dual-CDC concurrency | **PASS** | User `Serial` streams on the user CDC while a debug session runs on the service CDC over the same cable; pause/resume and host→user RX don't wedge the debug link. |
| Upload during a debug session | **PASS** | The debug bridge yields the service COM on an upload request (both before a client connects and mid-session); the halted-stub 1200-touch then reboots to the bootloader and the flash completes. |
| Repeated Upload clicks | **PASS** | A per-port lock makes a duplicate upload fail fast before any touch; firmware is never corrupted. |
| Wrong-port (user CDC) upload | **PASS** | Selecting the user CDC (`MI_02`) is rejected with a clear message; the firmware is untouched. The firmware also ignores a 1200-touch on the user CDC by design. |
| `usbcdc=disabled` in-app upload | **PASS** | Hands-free in-app 1200-touch reboots and flashes on the single service CDC. Verified 3× back-to-back, plus `usbcdc=disabled⇄enabled` transitions in both directions. |
| UF2 upload from DFU mode | **PASS** | `bootloader=auto` and explicit `bootloader=promicro` both match the selected board's UF2 drive and complete `Upload complete`. |
| Explicit serial DFU from DFU mode | **PASS** | `bootloader=promicroserial` enters `Starting Adafruit serial DFU transfer` and completes even while another UF2 drive is mounted. |
| UF2 drive-only helper | **PASS** | `uploadmode=uf2boot` leaves the selected board mounted as UF2 and exits with `Upload skipped`. |
| Multi-board UF2 disambiguation | **PASS** | Two `NICENANO` volumes were mounted simultaneously; board1 matched `J:\` by stable ID and board2 matched `K:\`. Stale COM selection is rejected. |

## ⚠️ Known limitations

- **Raw `adafruit-nrfutil dfu serial`** is less robust than `arduino-cli upload` on this clone because it lacks the package's port identity checks, retries, and stale-COM guard.

## ⏱️ Upload timing (measured)

Typical wall-clock for `arduino-cli upload` of a precompiled sketch, ProMicro clone on Windows:

| Phase | Time | Nature |
|---|---|---|
| Pre-touch port/identity checks | ~2.0 s | optimized (PnP snapshot cache; was ~5.1 s) |
| Touch → bootloader enumerated | ~3.7 s | firmware reboot + bootloader USB enum (floor) |
| UF2/DFU transfer | ~6-7 s | bootloader flash/write bound for the small smoke sketches |
| `arduino-cli` port enumeration wrapper | ~9 s | arduino-cli's own scan; the IDE does less with a pre-selected port |

The package-controllable pipeline (`upload.ps1`) runs in ~14–15 s; the rest is firmware/bootloader and arduino-cli floors.

## How to reproduce

### First flash (manual bootloader entry, once)

If the board is in an unknown state, enter the bootloader manually once: short `RST` to `GND` twice. A UF2 drive appears and the service COM (e.g. `COM3`) becomes visible. Then upload normally.

### Hands-free reupload smoke test

```powershell
arduino-cli compile --fqbn arduinonrf:nrf52:promicro_nrf52840:uploadmode=usb,bootloader=auto,usbcdc=enabled <sketch>
arduino-cli upload  -p COMx --fqbn arduinonrf:nrf52:promicro_nrf52840:uploadmode=usb,bootloader=auto,usbcdc=enabled <sketch>
```

Expected: upload completes with `Soft reset : done - board rebooted into new firmware`. If the board is already in DFU mode, use the current DFU/SERVICE COM shown by Arduino IDE; if it has returned to application mode, use the current service COM. Running the same command again must also succeed hands-free.

### UF2 drive-only smoke test

```powershell
arduino-cli upload -p COMx --fqbn arduinonrf:nrf52:promicro_nrf52840:uploadmode=uf2boot,bootloader=auto <sketch>
```

Expected: the selected board reports `UF2 drive ready`, prints the matched drive, and exits with `Upload skipped : board left in bootloader`.

### Single-cable debug smoke test

Build the `examples/UsbGdbStubBreakpoint` sketch with the `usbgdbstub` build profile and press **Debug** in Arduino IDE 2 (or attach `cortex-debug` to the bridge's TCP port). The board halts at the `bkpt #0` in `setup()`; stepping, registers, and breakpoints work over the single USB cable. See [platform/ARDUINO_IDE2_USB_GDBSTUB.md](platform/ARDUINO_IDE2_USB_GDBSTUB.md).
