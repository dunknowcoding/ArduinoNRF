# Upload Behavior

Date: 2026-08-17

This document records the current upload truth exposed by the package. It does not claim that every clone board is fully verified.

## Native USB group

These boards currently declare USB-backed upload support in package metadata:

- `promicro_nrf52840`
- `nicenano_v2`
- `supermini_nrf52840`
- `nrfmicro_nrf52840`
- `mini_nrf52840`
- `xiao_nrf52840`
- `devboard_nrf52840`
- `pitaya_go_nrf52840`
- `usb_dongle_nrf52840`

## SWD upload choices

Boards with SWD upload support expose explicit Arduino IDE **Upload Method**
entries for the probe type:

- `SWD programmer (CMSIS-DAP)` uses OpenOCD with `tools/openocd/nrf52-cmsis-dap.cfg`.
- `SWD programmer (SEGGER J-Link)` uses SEGGER `JLink.exe` through `upload.ps1`.

These entries are for the normal **Upload** button. They do not depend on the
IDE **Tools -> Programmer** selection, because Arduino upload recipes use the
board's selected `Upload Method` properties. Use **Tools -> Programmer** for
**Sketch -> Upload Using Programmer** and **Tools -> Burn Bootloader**.
`SEGGER J-Link (SWD)` uses SEGGER's command-line tools; `CMSIS-DAP (SWD)` uses
OpenOCD for locked-target recover. For locked nRF52 parts, the recover/erase
step is what clears `APPROTECT`; the bootloader HEX is flashed afterward.

Arduino IDE 2 SWD Debug follows the selected `Upload Method`: CMSIS-DAP keeps
the OpenOCD server, while the J-Link upload method switches the debug server
metadata to Arduino IDE's `jlink` server type.

The `devboard_nrf52833` target is SWD-first and exposes only these SWD upload
choices because it has no native USB upload path in this package.

## Current Windows wrapper behavior

- `tools/niusrobotlab/upload.ps1` owns the touch/reset sequence on Windows instead of relying on the Arduino CLI default touch path.
- Arduino board recipes keep `upload.use_1200bps_touch=false`; this prevents
  Arduino CLI from issuing a second, unscoped touch before the identity-aware
  wrapper runs.
- `Bootloader / DFU → Auto-detect` prefers a matching UF2 mass-storage volume when the selected board is already in bootloader mode. Explicit serial-DFU menu entries still use the packaged serial-DFU tool.
- UF2 drives are matched to the selected serial port by stable USB identity. If two boards expose the same volume label, the wrapper refuses ambiguous matches instead of choosing the first drive.
- For bootloaders whose USB PID differs from the application PID, the wrapper
  accepts the selected board's scoped UF2 volume as transition proof. It never
  waits for the old runtime COM to return before copying firmware; that COM can
  only return after the copy and reboot have completed.
- On Windows, runtime verification reads only the expected VID/PID branch and
  confirms that its COM endpoint is currently present. Stale registry records
  are rejected, and the full PnP-provider scan remains a compatibility fallback.
  The UF2 evidence observed during touch is reused instead of rediscovering the
  same volume before transfer.
- When a UF2 volume is visible, `upload.ps1` reads `INFO_UF2.TXT` and uses the
  `SoftDevice` field to infer the mounted layout (`0x1000`, `0x26000`, or
  `0x27000`). If that layout conflicts with the app start used by the selected
  Arduino IDE `Bootloader / DFU` option, upload fails before copying firmware.
  This applies to **UF2 deploy**, **serial DFU**, and the **UF2→serial fallback**
  path. Serial-only bootloaders do not expose mass storage, so that path makes
  an immediate identity-scoped `INFO_UF2.TXT` check when a matching volume is
  already available, without delaying an otherwise valid serial transfer. When
  no UF2 metadata exists, the exact app start and SoftDevice requirement come
  from the selected board recipe. A discovered mismatch remains terminal before
  firmware is written; Arduino compiles before upload, so the wrapper cannot
  safely relocate an already-linked image.
- Before any Windows USB discovery or touch, the uploader independently validates every
  Intel HEX record checksum and length, rejects overlaps or records after EOF,
  and requires the actual vector table, reset entry, start address, and highest
  programmed byte to fit the selected application range. The same validated
  image is used for serial DFU and UF2; recipe metadata alone is not accepted as
  proof that the compiled image is safe to transfer.
- **Layout guard** (`layout` failure): compares the IDE `Bootloader / DFU` app
  start (`0x1000` / `0x26000` / `0x27000`) against `INFO_UF2.TXT` on the
  selected board's UF2 drive (matched by stable USB identity, not drive letter).
  Disable with `NIUS_DISABLE_LAYOUT_GUARD=1` (not recommended).
- **Misflash guard** (`misflash` failure): after serial DFU or direct UF2, waits
  for the selected board to return in application mode. The fast path checks
  the expected VID/PID and preserved physical-device identity without opening
  the newly enumerated application COM. If USB never comes back
  (typical when app start was wrong), the wrapper attempts 1200 bps touch / UF2
  recovery and fails with an IDE-visible message. Disable with
  `NIUS_DISABLE_MISFLASH_GUARD=1`.
  A reused COM number is not sufficient evidence: the endpoint must match the
  board recipe's runtime VID/PID. This prevents the departing bootloader CDC
  from being mistaken for a successfully started application.
- **Manual UF2 drag in DFU mode is not guarded.** Copying a `.uf2` from Explorer
  bypasses `upload.ps1`. You must match **bootloader layout**, not just the
  version string in `INFO_UF2.TXT` (for example `0.6.0` exists in both S140
  `@0x26000` and no-SoftDevice `@0x1000` variants). Sketch UF2 must match the
  mounted layout. Adafruit **update-*** bootloader packages (family
  `0xd663823c`) rewrite the bootloader and reboot into application mode; they
  are not sketch images. After a layout switch, also flash a matching app or USB
  may disappear. See [../bootloaders/README.md](../bootloaders/README.md).
- **USB silent / COM missing after a bad or partial flash:** the host cannot
  1200-touch a port that is gone. Recovery is manual: **double-tap RESET** to
  re-enter UF2 (on boards **without** a reset button, short **RST to GND twice**
  quickly, like a double-tap), then fix the `Bootloader / DFU` menu and upload
  again; or recover over **SWD** (`Tools → Programmer → SEGGER J-Link (SWD)` or
  CMSIS-DAP, then **Burn Bootloader** / sketch upload). See
  [../bootloaders/README.md](../bootloaders/README.md).
- `Upload Method → Enter UF2 drive only (no upload)` performs the touch/bootloader wait, reports the matched drive, and exits before copying firmware.
- If the selected upload COM is stale after a mode change, the wrapper fails with a clear "re-select the current SERVICE/DFU port" message instead of falling through to another board.
- ProMicro-class application firmware uses runtime PID `0x00B4`; PID `0x00B3`
  belongs to the UF2 bootloader. Keep this split even for no-SoftDevice builds:
  the bootloader exposes interface 2 as mass storage, while TaichiUSB exposes it
  as the user CDC port. Reusing one PID and chip serial for both descriptor
  layouts makes Windows retain the wrong per-interface driver binding.
- After serial DFU, the upload wrapper accepts the selected runtime interface
  when it returns with the expected runtime VID/PID and physical-device
  identity. Windows
  may assign a different number when the bootloader and application expose
  different composite interfaces; that remap is not an upload failure.
- The runtime DFU interface is hidden by default. Hands-free upload uses the
  service CDC's 1200-bps touch, so the extra driverless DFU-runtime node is not
  needed for ordinary use. Enable it explicitly only for a workflow that sends
  USB DFU runtime requests directly.
- The previous service-port "boot token" fallback (`~NIUSBL!42\r` after arming with line coding `134/8/2/2 + DTR+RTS`) has been removed — the standard 1200 bps touch path is now the single primary trigger.

## Linux and macOS wrapper behavior

- `upload.py` accepts the selected port only when it matches either the board
  recipe's runtime identity or its bootloader identity; the script then owns
  the 1200-bps transition through `adafruit-nrfutil`.
- The script validates the actual Intel HEX framing, vector table, link address,
  and maximum application range before resolving or touching a USB device.
- On a dual-CDC runtime, a selected user endpoint is remapped to interface zero
  only when Linux sysfs or the macOS IOUSB registry proves it is a sibling on
  the same USB composite. Ambiguous mappings fail closed.
- A successful transfer is not sufficient by itself. When available, Linux sysfs or
  the macOS IOUSB registry must return the selected physical USB topology with the
  declared runtime VID/PID before upload succeeds. Exact USB serial is the fallback
  when topology is unavailable, so serial-less boards and bootloader/runtime serial
  changes remain scoped without allowing a peer board to satisfy the check.
- The same runtime identity must remain continuously present for 300 ms by default;
  a transient enumeration followed by an early application/watchdog failure is not
  reported as upload success. `--runtime-stable-ms` may tune this bounded check.
- One host-local advisory lock owns a physical topology/serial identity throughout
  package generation, transfer, and runtime verification. A concurrent uploader
  fails before touching that target, while the OS releases the lock after exit.
- USB IDs, address ranges, process/runtime timeouts, DFU/touch baud rates, device
  type, and SoftDevice requirement are finite and range-checked before transfer;
  malformed touch input cannot silently disable the bootloader transition.
- Ambiguous same-identity devices fail closed instead of allowing a peer board
  to satisfy target detection or post-upload verification.
- Generic boards whose base recipe says `auto` must select an explicit
  **Bootloader / DFU** identity on Linux/macOS; the cross-platform uploader does
  not guess a bootloader VID/PID after the application port disappears.

## Probe and debugger image safety

- The link recipe constrains ELF load-segment alignment to the nRF flash page
  size. An application linked for `0x0`, `0x1000`, `0x26000`, or `0x27000` therefore
  has its first file-backed `PT_LOAD` segment at that exact address; debugger
  tools cannot interpret alignment padding as an earlier flash load and erase a
  SoftDevice prefix.
- A debugger should still validate ELF program headers, not only section
  addresses, and protect the MBR/SoftDevice plus bootloader ranges in its target
  profile. Arduino USB upload continues to use the independently validated HEX
  image.
- Startup, SoftDevice, fault, and USB diagnostic words live in the linker's
  `.noinit` allocation. No diagnostic uses a guessed absolute RAM address.
- No-SoftDevice profiles link data at `0x20000000` and expose the full 256 KiB
  RAM. SoftDevice profiles retain the required `0x20006000` RAM origin.

## USB-safe idle and power-down behavior

- Native USB remains interrupt-driven during ordinary System ON `WFI` idle;
  applications do not need to busy-wait merely to preserve enumeration.
- Generic `nrfSystemPowerDown()` refuses SystemOFF whenever VBUS is present,
  including the pre-configuration and USB-suspend windows. Entering SystemOFF
  in either window can leave no new VBUS edge to wake the MCU.
- `NrfPower::enterSystemOff()` applies the same safe default. A caller that
  intentionally accepts disconnecting an already powered USB session must say
  so explicitly with `enterSystemOff(true)`.

## Real Promicro-class board result

### What is now working

- first upload from manual bootloader mode works on the user's board
- the board returns to user mode afterward
- with `usbcdc=disabled`, the board keeps a single visible SERVICE CDC path
- UF2 upload from the current DFU port works in both `bootloader=auto` and explicit UF2 menu modes
- explicit Adafruit serial DFU from the current DFU port works and is not confused by a mounted UF2 volume
- with two boards simultaneously mounted as `NICENANO`, the selected board maps to its own volume by stable USB identity
- selecting a stale COM after the board re-enumerates is rejected before any upload can target another board
- **a second upload from user mode works** — the 1200 bps touch triggers `NVIC_SystemReset()` into the bootloader, the selected transport streams the image, and the board re-boots into user mode.

### What previously failed (historical)

Before the V1 firmware fixes, `adafruit-nrfutil` would stall at
`Sending DFU start packet` during the second upload because the firmware
never re-entered bootloader in response to the 1200 bps touch. The host then
saw `Port never detached after touch (port stayed present)` repeatedly across
the four host-side trigger mechanisms upload.ps1 used to try.

Root cause: three cooperating firmware bugs in `NrfUsbd.cpp` — EP0 OUT
EasyDMA never triggered, EP0 OUT direction routed by stale `BMREQUESTTYPE`,
and a subsequent DTR=true cancelling `serviceTouchPending_` inside the 40 ms
confirm window.

### `usbcdc=disabled` in-app upload (host-side fix)

For a while, an in-app upload to a `usbcdc=disabled` board stalled at
`adafruit-dfu` even though the firmware touch path was correct. Root cause was
host-side: with no user CDC, the runtime DFU **"Bootloader Control"** vendor
interface lands on `MI_02` (where `usbcdc=enabled` puts the user CDC). `upload.ps1`
counted any non-Ports `MI_02` interface as MSC/bootloader evidence, decided the
board was *already* in the bootloader, **skipped the 1200-touch entirely**, and
ran `adafruit-nrfutil` against the still-running app. The fix excludes the
`Bootloader Control` interface from that evidence (the real bootloader is
identified by its UF2 mass-storage volume, never by this control interface), so
the touch runs and the board reboots normally. Verified 3× back-to-back plus
`usbcdc` transitions in both directions.

### What still needs validation

- Boards beyond the user's ProMicro clone that share the firmware path
- Linux/macOS UF2 parity; those platforms still use the Python/Adafruit serial-DFU recipe
