# Bootloader Notes

Date: 2026-06-11

## Bootloader families in this package

### UF2 mass storage

UF2 is now a first-class Windows upload path for boards that expose a UF2 drive. `upload.ps1` converts the sketch HEX to UF2, matches the drive to the selected upload COM by stable USB identity, copies the file, and resets the board.

The **Upload Method -> Enter UF2 drive only (no upload)** menu is for inspection/recovery: it enters bootloader mode, reports the matched drive, and stops before copying firmware.

### Adafruit serial DFU

Adafruit serial DFU remains available through explicit bootloader menu entries such as `Serial DFU, SoftDevice BLE`. On Windows the package uses `upload.ps1`; on Linux/macOS it uses `upload.py` plus `adafruit-nrfutil`.

Boards packaged on this family include the validated AliExpress ProMicro nRF52840 path plus the modeled nice!nano v2, SuperMini, nRFMicro, XIAO, and Pitaya Go definitions. See [../uploads/hands_free_upload.md](../uploads/hands_free_upload.md) and [../COMPATIBILITY.md](../COMPATIBILITY.md).

### Nordic Open DFU

The `usb_dongle_nrf52840` target (PCA10059) uses Nordic Open DFU, not Adafruit serial DFU. Use Nordic tooling such as `nrfutil` or nRF Connect for Desktop rather than the package's default `niusdfu` flow for that board.

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
driver installed. CMSIS-DAP paths continue to use OpenOCD.

Arduino IDE 2 SWD Debug follows the selected **Upload Method**: CMSIS-DAP keeps
the OpenOCD server, while the J-Link upload method switches the debug metadata
to Arduino IDE's `jlink` server type.

Arduino IDE **Tools -> Burn Bootloader** is available for the nice!nano-family
definitions that have a known bundled bootloader image:

- `promicro_nrf52840`
- `nicenano_v2`
- `supermini_nrf52840`

Select **Tools -> Programmer -> SEGGER J-Link (SWD)** or **CMSIS-DAP (SWD)**
first. The wrapper performs a chip erase/recover and then programs the bundled
nice!nano bootloader HEX. J-Link uses SEGGER `JLink.exe`; CMSIS-DAP uses
OpenOCD `nrf52_recover`.

`hardware/arduinonrf/nrf52/bootloaders/nice_nano/nice_nano_bootloader-0.6.0_s140_6.1.1.hex`

After burning this image, select a S140 v6 / `0x26000` Bootloader / DFU layout
before compiling sketches. Do not use the no-SoftDevice `0x1000` menu entries
with this bootloader image.

Burn Bootloader is intentionally **not** exposed as a single-USB operation. If
the existing bootloader still works, USB DFU bootloader updates can be done with
the vendor DFU package, but IDE Burn Bootloader is the recovery path and requires
SWD.

### In-field UF2 bootloader update (no SoftDevice / nice!nano)

Bundled under `hardware/arduinonrf/nrf52/bootloaders/nice_nano/`:

- `update-nice_nano_bootloader-0.6.0_nosd.uf2` — Adafruit **update** UF2
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

Before you drop a file in DFU/UFU mode:

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
