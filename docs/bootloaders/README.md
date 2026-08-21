# Bootloader Notes

Date: 2026-06-11

## Bootloader families in this package

### UF2 mass storage

UF2 is now a first-class Windows upload path for boards that expose a UF2 drive. `upload.ps1` converts the sketch HEX to UF2, matches the drive to the selected upload COM by stable USB identity, copies the file, and resets the board.

The **Upload Method -> Enter UF2 drive only (no upload)** menu is for inspection/recovery: it enters bootloader mode, reports the matched drive, and stops before copying firmware.

### Adafruit serial DFU

Adafruit serial DFU remains available through explicit bootloader menu entries such as `Serial DFU, SoftDevice BLE`. On Windows the package uses `upload.ps1`; on Linux/macOS it uses `upload.py` plus `adafruit-nrfutil`.

Boards packaged on this family include the validated AliExpress ProMicro nRF52840 path plus the modeled nice!nano v2, SuperMini, nRFMicro, XIAO, and Pitaya Go definitions. See [../uploads/hands_free_upload.md](../uploads/hands_free_upload.md) and [../COMPATIBILITY.md](../COMPATIBILITY.md).

## INFO_UF2 layout reference (version → IDE menu)

Use this table when the board is in UF2/DFU mode and you can read `INFO_UF2.TXT`
on the mounted drive. **Pick the row by `SoftDevice` first.** The
`UF2 Bootloader X.Y.Z` line is a secondary cross-check only — community and
retail images often share the same version string across different flash
layouts (for example `0.6.0` on both S140 `@0x26000` and no-SoftDevice
`@0x1000` builds).

On Windows, **Sketch → Upload** with `upload.ps1` enforces the same layout match
before writing firmware. **Explorer drag-and-drop does not** — use this table
before any manual `.uf2` copy.

### Adafruit / nice!nano-class UF2 bootloaders (VID `0x239A`, typical clone PID `0x00B3`)

| `INFO_UF2.TXT` **`SoftDevice`** | Typical **`UF2 Bootloader`** lines *(online / retail)* | App start | **Tools → Bootloader / DFU** menu label *(exact IDE text)* |
|---|---|---:|---|
| `S140 version 6.1.1` or other **S140 6.x** wording | `UF2 Bootloader 0.6.0`; also reported: `0.7.0`, `0.8.0`, `0.11.0`, … | `0x26000` | **Auto-detect upload, SoftDevice S140 v6 layout (0x26000)** |
| same S140 6.x row | same version strings | `0x26000` | **UF2 mass storage, SoftDevice S140 v6 layout (0x26000)** |
| same S140 6.x row | same version strings | `0x26000` | **Serial DFU, SoftDevice S140 v6 layout (0x26000)** |
| **`not found`** | `UF2 Bootloader 0.6.0`; also reported: `0.7.0`, … *(nosd / MBR-only)* | `0x1000` | **Auto-detect upload, no SoftDevice / MBR only (0x1000)** |
| **`not found`** | same as above | `0x1000` | **UF2 mass storage, no SoftDevice / MBR only (0x1000)** |
| **`not found`** | same as above | `0x1000` | **Serial DFU, no SoftDevice / MBR only (0x1000)** |
| **S140 version 7.x** or legacy S140 strings | varies by vendor | `0x27000` | **UF2 mass storage, SoftDevice S140 v7 / legacy layout (0x27000)** |
| S140 v7 / legacy row | varies | `0x27000` | **Serial DFU, SoftDevice S140 v7 / legacy layout (0x27000)** |

**How to apply the table**

1. Enter UF2/DFU (double-tap RESET, 1200-bps touch from Upload, or **Upload Method → Enter UF2 drive only**).
2. Open `INFO_UF2.TXT` on the **selected board's** UF2 volume (match stable USB identity when several `NICENANO` drives are mounted — never assume a fixed drive letter).
3. Find the **`SoftDevice`** row above.
4. In the IDE, set **Tools → Bootloader / DFU** to the matching menu label **before Compile / Upload**.
5. Use the **`Auto-detect …`** entry when unsure and the `SoftDevice` line matches; use an explicit **UF2 mass storage …** or **Serial DFU …** entry when you want to force that transport.

**Worked examples**

| What you read in `INFO_UF2.TXT` | Correct IDE choice |
|---|---|
| `SoftDevice: S140 version 6.1.1` and `UF2 Bootloader 0.6.0` | **Auto-detect upload, SoftDevice S140 v6 layout (0x26000)** |
| `SoftDevice: not found` and `UF2 Bootloader 0.6.0` | **Auto-detect upload, no SoftDevice / MBR only (0x1000)** |
| `SoftDevice: S140 version 6.1.1` and `UF2 Bootloader 0.11.0` | still **S140 v6 layout (0x26000)**; version went up, layout did not |
| `SoftDevice: not found` and `UF2 Bootloader 0.11.0` | still **no SoftDevice / MBR only (0x1000)** |

**Not covered by this table**

| Item | Notes |
|---|---|
| Adafruit **`update-*` bootloader UF2** (family `0xd663823c`) | Bootloader replacement package, not a sketch. Changes MBR/bootloader/UICR; reboots into app mode. See [In-field UF2 bootloader update](#in-field-uf2-bootloader-update-no-softdevice--nicenano) below. |
| **Tools -> Burn Bootloader** (SWD) | Package ships board-specific SWD bootloader images. For `promicro_nrf52840`, the default S140 recovery image is `promicro_nrf52840_bootloader-0.9.2_s140_6.1.1.hex`; its no-SoftDevice recovery image is `promicro_nrf52840_bootloader-0.11.0_nosd.hex`. For nice!nano-family entries, burn `nice_nano_bootloader-0.6.0_s140_6.1.1.hex`. After burning an S140 v6 image, use the **S140 v6 `(0x26000)`** rows above; after burning a noSD image, use a **no SoftDevice `(0x1000)`** row. |
| **Nordic PCA10059 USB serial DFU** (`usb_dongle_nrf52840`) | No UF2 volume. Uses the package's Secure-DFU serial transport with app start `0x1000` and no-SoftDevice requirement `0x00`. |
| **Seeed XIAO** UF2 labels | Volume label / model differ (`XIAO-BOOT`, etc.) but S140 v6/v7 **app-start rules are the same**; pick the matching `(0x26000)` / `(0x27000)` / `(0x1000)` entry for your board definition. |

If a future community bootloader introduces a **new app start** not listed here, add
a new **Bootloader / DFU** menu entry in the board definition following the
existing naming pattern (`<transport>, <SoftDevice or layout note> (0xXXXX)`)
and do not add a parallel menu axis keyed only on `UF2 Bootloader X.Y.Z`.

### Nordic PCA10059 USB serial DFU

The `usb_dongle_nrf52840` target (PCA10059) uses Nordic Secure DFU over its
USB CDC bootloader interface (`0x1915:0x521F`), not the USB DFU class handled by
`dfu-util`. The package therefore generates a standard application DFU package
with no-SoftDevice requirement `0x00` and sends it through the serial-DFU path.
The MBR reserves `0x0000..0x0FFF`; the application starts at `0x1000`, package
storage occupies the final 16 KiB below `0xE0000`, and the onboard bootloader
remains untouched at `0xE0000..0xFFFFF`.

### SWD / J-Link / OpenOCD

Generic dev boards, official Nordic DK-style hardware, and any board exposing usable pads can be flashed over SWD.

For normal sketch upload, choose the probe directly from **Tools -> Upload
Method**:

- `SWD programmer (CMSIS-DAP)`
- `SWD programmer (SEGGER J-Link)`

For **Sketch -> Upload Using Programmer** and **Tools -> Burn Bootloader**,
choose the probe from **Tools -> Programmer** instead.

On Windows, the J-Link paths use SEGGER's command-line tools. This avoids the
common OpenOCD/libusb failure mode on machines that have the official SEGGER
driver installed. CMSIS-DAP paths use OpenOCD.

Arduino IDE 2 SWD Debug follows the selected **Upload Method**: CMSIS-DAP keeps
the OpenOCD server, while the J-Link upload method switches the debug metadata
to Arduino IDE's `jlink` server type.

Arduino IDE **Tools -> Burn Bootloader** is available for the nice!nano-family
definitions that have a known bundled bootloader image:

- `promicro_nrf52840`
- `nicenano_v2`
- `supermini_nrf52840`

Select **Tools -> Programmer -> SEGGER J-Link (SWD)** or **CMSIS-DAP (SWD)**
first. For `promicro_nrf52840`, also select the matching **Bootloader / DFU**
layout before burning: S140 entries keep the default S140 HEX; no-SoftDevice
entries select the packaged noSD recovery HEX. The wrapper performs a chip
erase/recover and then programs the bundled board-specific bootloader HEX.
J-Link uses SEGGER `JLink.exe`; CMSIS-DAP uses OpenOCD `nrf52_recover`. The
recover step clears `APPROTECT`; the bootloader HEX must leave APPROTECT erased
(`0xFF`) or unmentioned so recovery remains possible.

The wrapper validates the complete recovery image before that destructive step:
Intel HEX checksums/ordering/EOF, selected flash and UICR limits, MBR and
bootloader vectors, target SRAM ceiling, page-aligned UICR bootloader address,
and erased `APPROTECT` must all agree. Windows, Linux, and macOS use this same
pre-erase contract. **Upload Using Programmer** likewise retains the normal
application vector/layout preflight instead of bypassing it with raw OpenOCD.
On multi-probe hosts, set `NIUS_JLINK_SERIAL` or `NIUS_CMSIS_DAP_SERIAL` to the
exact probe serial/nickname before starting the IDE operation.

- `hardware/arduinonrf/nrf52/bootloaders/promicro_nrf52840/promicro_nrf52840_bootloader-0.9.2_s140_6.1.1.hex`
- `hardware/arduinonrf/nrf52/bootloaders/promicro_nrf52840/promicro_nrf52840_bootloader-0.11.0_nosd.hex`
- `hardware/arduinonrf/nrf52/bootloaders/nice_nano/nice_nano_bootloader-0.6.0_s140_6.1.1.hex`

After burning an S140 image, select a S140 v6 / `0x26000` Bootloader / DFU
layout before compiling sketches. After burning the ProMicro noSD image, select
a no-SoftDevice / `0x1000` Bootloader / DFU layout before compiling sketches.

Burn Bootloader is intentionally **not** exposed as a single-USB operation. If
the existing bootloader still works, USB DFU bootloader updates can be done with
the vendor DFU package, but IDE Burn Bootloader is the recovery path and requires
SWD.

### When SWD recover fails with a clone J-Link (`J-Link (unknown)`)

If Burn Bootloader / `recover` fails on an nRF52840 whose debug access is locked
(**APPROTECT**), first check what probe you have. Symptom of the trap:

- The probe enumerates and reports `VTref` ≈ 3.3 V (the board **is** powered), but
  every attach ends in `Could not connect to the target device` after the
  "Reset via Reset pin & Connect" fallback.
- `JLink.exe` prints `Firmware: J-Link ARM-OB STM32 compiled Aug 22 2012` (or
  similar old OB build) and SEGGER software labels it `J-Link (unknown)`,
  serial `123456` — i.e. a **clone J-Link OB**.

**Cause:** the clone's firmware requires a successful, core-halting
`Connect(device)` before it will service *any* raw SWD/CoreSight transfer. With
APPROTECT enabled the halting connect faults on the walled-off AHB-AP, so the
clone can never write **DP SELECT** to reach the **CTRL-AP** and run the nRF52
`ERASEALL` recovery. A genuine SEGGER J-Link does that recovery inside its DLL;
the clone cannot, and neither can J-Link Commander `recover`/`Erase`,
pyOCD-over-J-Link, nor STM32CubeProgrammer's bundled `JLink_x64.dll`.

**Fix — use a raw-SWD probe instead** (for example CMSIS-DAP, or a genuine
SEGGER J-Link whose own DLL handles recover), then run the CTRL-AP mass-erase
that clears APPROTECT:

```bash
# OpenOCD (the Burn Bootloader CMSIS-DAP path already uses this target):
openocd -f interface/cmsis-dap.cfg -f target/nrf52.cfg -c "init; nrf52_recover; exit"
# or pyOCD:
pyocd erase -t nrf52840 --mass    # CTRL-AP ERASEALL over CMSIS-DAP/raw-SWD
```

After the erase, APPROTECT is cleared and the debug port opens; re-run **Burn
Bootloader** (or flash the board-specific S140 bootloader HEX directly) and
select the S140 `0x26000` layout.

### In-field UF2 bootloader update (no SoftDevice / nice!nano)

Bundled under `hardware/arduinonrf/nrf52/bootloaders/nice_nano/`:

- `update-nice_nano_bootloader-0.6.0_nosd.uf2` - Adafruit **update** UF2
  (family `0xd663823c`). Rewrites MBR/bootloader/UICR for app start `0x1000`.
  This is **not** a sketch and **not** the same as **Burn Bootloader** over SWD.

Deploy with scoped tooling (never copy to a fixed drive letter when multiple
boards are connected):

```powershell
.\hardware\arduinonrf\nrf52\tools\niusrobotlab\deploy_uf2_bootloader_update.ps1 -Port COMx
```

**Expected behavior after update:** the board reboots into **application mode**,
not UF2. If the app region at `0x1000` is empty or still built for SoftDevice
(`0x26000`), USB may stay off until you double-tap RESET and flash a matching
no-SoftDevice sketch (`bootloader=promicronosduf2` / `autonosd`). The deploy
script compiles and copies `MinimalUsbSmoke` over UF2 automatically.

If USB is missing after update, double-tap RESET then re-run with
`-SkipBootloaderUpdate` and `-CompositeStableId <16-hex>` (when COM is gone).

See also `hardware/arduinonrf/nrf52/bootloaders/nice_nano/README.md`.

## Manual UF2 copy (Explorer drag-and-drop)

IDE **Upload** runs `upload.ps1`, which enforces layout guards. **Dragging a
`.uf2` onto a UF2 drive in Explorer does not.** Treat manual copy as expert
recovery only.

Before you drop a file in DFU/UF2 mode:

1. Open `INFO_UF2.TXT` on **that board's** drive (when several clones are
   connected, match by stable USB identity — do not assume a fixed drive letter).
2. Match **layout**, not just the bootloader version line. The same
   `UF2 Bootloader 0.6.0` string appears on S140 (`SoftDevice: S140 …`, app
   `@0x26000`) and no-SoftDevice (`SoftDevice: not found`, app `@0x1000`)
   builds.
3. **Sketch UF2** must be linked for the same app start as the mounted
   bootloader. Wrong layout may copy successfully but leave USB off after reset.
4. **`update-*` bootloader UF2** (Adafruit family `0xd663823c`) replaces the
   bootloader and reboots into **application mode**, not UF2. Plan a matching
   recovery app or expect to re-enter DFU manually.

### If COM / UF2 disappears after a manual or mismatched flash

The bootloader may still be running, but application USB is off. The PC cannot
send 1200 bps touch without a COM port. Recovery:

- **Double-tap RESET** to re-enter UF2 (fast press twice, like a double-click).
  Boards **without** a reset button: touch **RST to GND twice** quickly with a
  jumper or tweezers.
- Then flash a **matching** sketch (IDE Upload with the correct `Bootloader /
  DFU` menu, or a correct sketch UF2 copy), or use scoped tooling such as
  `deploy_uf2_bootloader_update.ps1`.
- If USB still does not return, use **SWD**: `Tools → Programmer → SEGGER
  J-Link (SWD)` or CMSIS-DAP, then **Burn Bootloader** and/or sketch upload
  over SWD.

Windows IDE Upload details: [../platform/UPLOAD_BEHAVIOR.md](../platform/UPLOAD_BEHAVIOR.md).

## Current package assumption

The package assumes either USB DFU or SWD-based upload depending on the board
definition. Bootloader replacement is limited to boards with an explicitly
configured, bundled bootloader image and is always SWD-only.
