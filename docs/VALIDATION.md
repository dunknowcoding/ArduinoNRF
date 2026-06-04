# Hardware validation status

What has been verified on real hardware, and how to reproduce it. Reference board: **AliExpress ProMicro nRF52840 clone** (bootloader family `0x239A:0x00B3`, app start `0x1000`), build `bootloader=promicroserialnosd,usbcdc=enabled`.

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

## ⚠️ Known limitations

- **Raw `adafruit-nrfutil dfu serial`** is unreliable on this clone; the robust path is `arduino-cli upload` (which `upload.ps1` drives with retries and timing).

## ⏱️ Upload timing (measured)

Typical wall-clock for `arduino-cli upload` of a precompiled sketch, ProMicro clone on Windows:

| Phase | Time | Nature |
|---|---|---|
| Pre-touch port/identity checks | ~2.0 s | optimized (PnP snapshot cache; was ~5.1 s) |
| Touch → bootloader enumerated | ~3.7 s | firmware reboot + bootloader USB enum (floor) |
| DFU transfer (~82 KB) | ~6.4 s | DFU-protocol/flash bound (floor) |
| `arduino-cli` port enumeration wrapper | ~9 s | arduino-cli's own scan; the IDE does less with a pre-selected port |

The package-controllable pipeline (`upload.ps1`) runs in ~14–15 s; the rest is firmware/bootloader and arduino-cli floors. See [platform/USB_1200_TOUCH_V1_FIX.md](platform/USB_1200_TOUCH_V1_FIX.md) for the firmware fixes that make the touch work.

## How to reproduce

### First flash (manual bootloader entry, once)

If the board is in an unknown state, enter the bootloader manually once: short `RST` to `GND` twice. A UF2 drive appears and the service COM (e.g. `COM3`) becomes visible. Then upload normally.

### Hands-free reupload smoke test

```powershell
arduino-cli compile --fqbn arduinonrf:nrf52:promicro_nrf52840:bootloader=promicroserialnosd,usbcdc=enabled <sketch>
arduino-cli upload  -p COM3 --fqbn arduinonrf:nrf52:promicro_nrf52840:bootloader=promicroserialnosd,usbcdc=enabled <sketch>
```

Expected: upload completes with `Soft reset : done - board rebooted into new firmware` and the board returns to user mode on the same COM, with **no manual reset**. Running the same command again must also succeed hands-free.

### Single-cable debug smoke test

Build the `examples/UsbGdbStubBreakpoint` sketch with the `usbgdbstub` build profile and press **Debug** in Arduino IDE 2 (or attach `cortex-debug` to the bridge's TCP port). The board halts at the `bkpt #0` in `setup()`; stepping, registers, and breakpoints work over the single USB cable. See [platform/ARDUINO_IDE2_USB_GDBSTUB.md](platform/ARDUINO_IDE2_USB_GDBSTUB.md).
