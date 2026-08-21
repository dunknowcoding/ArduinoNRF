# Hands-free upload (1200-bps touch → UF2 / serial DFU)

The default upload path for every Adafruit-fork board in this package. Plug a USB cable in, click **Upload**, and the new firmware is on the board — no button press, no manual bootloader entry.

On Windows, `upload.ps1` handles UF2 mass-storage bootloaders, Adafruit serial
DFU, and the PCA10059 Nordic USB-serial DFU protocol. With **Bootloader / DFU
→ Auto-detect**, a mounted UF2 drive for the selected board is preferred;
explicit serial-DFU menu entries force the serial path.

## How it works

1. The platform uploader resolves the selected runtime interface to the exact
   **service / maintenance CDC**, opens it at **1200 baud**, and immediately
   closes it (the long-standing Arduino "1200-bps touch").
2. The firmware sees the touch and triggers `SYSRESETREQ` with the bootloader magic in `GPREGRET`, rebooting straight into the bootloader.
3. If a matching UF2 drive is present, the wrapper converts the sketch HEX to UF2 and copies it to that drive. If an explicit serial-DFU bootloader menu is selected, `adafruit-nrfutil dfu serial` streams the packaged firmware over the bootloader CDC instead.
4. The bootloader resets back to the new app.

## Required board settings

`boards.txt` for the board family enables:

```text
upload.tool=niusdfu
upload.use_1200bps_touch=false      # the identity-aware wrapper owns the touch
upload.wait_for_upload_port=false   # the wrapper owns the bounded port wait
upload.bootloader_mode=auto         # Windows auto-detects UF2 vs serial DFU
```

Arduino's generic transition hooks stay disabled for these recipes. The
platform uploader owns the single 1200-bps transition, identity-scoped port
mapping, bounded wait, transfer, and post-upload runtime verification. This
prevents a second touch or redundant unscoped wait from racing the verified
pipeline. Windows uses `upload.ps1`; Linux and macOS use the stdlib-only
`upload.py` implementation of the same ownership rule.

## Windows — `upload.ps1`

A hardened PowerShell pipeline drives the touch and DFU. Beyond the basics it provides:

- **Runtime-to-bootloader COM remapping by stable USB identity.** Windows may
  assign different COM numbers to the application and serial-DFU interfaces.
  The uploader waits for the selected board's bootloader identity, then opens
  that COM; it will not substitute another attached board just because that is
  the only bootloader currently visible.
- **Busy-port failure before mutation.** If another monitor owns the selected
  runtime/service CDC, upload stops before the 1200-bps request or any flash
  operation and names the occupied port.
- A per-port mutex so double-clicking **Upload** can't interleave two flashes.
- A bridge-yield IPC so an active debug session releases the COM for the upload.
- Identity-scoped user-to-service CDC resolution; ambiguous sibling or peer mappings fail before touch.
- A stale-port guard: if the selected COM disappeared after a bootloader transition, the upload fails clearly instead of matching another board.
- Stable-ID UF2 matching so two boards with the same volume label (for example two `NICENANO` drives) do not conflict.
- **Upload Method → Enter UF2 drive only (no upload)**, which leaves the selected board mounted as a UF2 drive and stops before copying firmware.
- A pre-touch PnP-snapshot cache (~3 s saved on the wall-clock).
- A **layout guard** on Windows: UF2 transfers require matching
  `INFO_UF2.TXT`; serial DFU checks the same identity-scoped metadata
  immediately when the bootloader exposes it. Serial-only bootloaders instead
  use the exact app-start and SoftDevice requirement in the selected recipe.
- Strict sketch-image preflight: Intel HEX checksums, record lengths, overlap,
  EOF, vector table, reset entry, link start, and maximum application range are
  validated before Windows inspects or touches USB and before Linux/macOS
  serial DFU touches the selected device.
- A **misflash guard** after every transfer that requires the expected runtime
  USB identity to return, without opening or holding the fresh application COM.

Manual UF2 drag-and-drop is **not** guarded; see
**[docs/platform/UPLOAD_BEHAVIOR.md](../platform/UPLOAD_BEHAVIOR.md)** and
**[docs/bootloaders/README.md](../bootloaders/README.md)**.

### Python used for UF2 conversion on Windows

The wrapper verifies that a discovered launcher actually starts Python 3. It
supports regular `python` / `python3` commands, Windows Store App Execution
Aliases, the Windows `py -3` launcher, standard per-user Python installs, and
Conda. If Python was installed while Arduino IDE was open, restart the IDE so
it inherits the updated `PATH`. As an explicit fallback, set
`NIUS_UF2_PYTHON_EXE` to the full path of `python.exe`; a virtual-environment
directory may instead be supplied through `NIUS_UF2_VENV`.

Driven by `tools.niusdfu.upload.pattern.windows` in `platform.txt`.

## Linux / macOS — `upload.py`

A small stdlib-only Python script that mirrors the proven Adafruit cross-platform recipe:

```text
adafruit-nrfutil dfu genpkg --dev-type 0x0052 --sd-req ... --application app.hex pkg.zip
adafruit-nrfutil dfu serial -pkg pkg.zip -p {port} -b 115200 --singlebank -t 1200
```

Driven by `tools.niusdfu.upload.pattern.linux` / `.macosx`. Requires `adafruit-nrfutil` on `PATH`:

On dual-CDC runtimes, Linux sysfs or the macOS IOUSB registry ties the selected
serial endpoint to its exact USB composite and resolves interface zero before
the 1200-bps touch. Post-upload verification accepts the returning composite
once its declared runtime identity and captured USB serial are present; it does
not mistake the two CDC interfaces for two boards.

```bash
pip3 install --user adafruit-nrfutil
# then ensure $HOME/.local/bin (Linux) or $HOME/Library/Python/3.X/bin (macOS) is on PATH
```

Linux also needs the shipped udev rules so non-root users can touch + flash:

```bash
sudo cp hardware/arduinonrf/nrf52/tools/niusrobotlab/99-arduinonrf.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
sudo usermod -a -G dialout $USER     # log out / back in
```

## Per-board status

See **[../COMPATIBILITY.md](../COMPATIBILITY.md)** for the full matrix. UF2 upload, explicit Adafruit serial DFU, the UF2-drive-only helper, and two-board UF2 volume disambiguation are verified end-to-end on the **AliExpress ProMicro nRF52840** clone. The same bootloader family is packaged for nice!nano v2, SuperMini, XIAO, Pitaya Go, and nRFMicro once the corrected VID:PIDs are in place, but those boards remain modeled / reference-core rather than re-verified on physical hardware in this revision.

## SWD upload from the IDE

For boards with SWD pads or an onboard debugger, use **Tools -> Upload Method**
to choose the probe used by the normal **Upload** button:

- `SWD programmer (CMSIS-DAP)`
- `SWD programmer (SEGGER J-Link)`

This is separate from **Tools -> Programmer**. The Programmer menu is used by
**Sketch -> Upload Using Programmer** and **Tools -> Burn Bootloader**, while
the normal Upload button follows the board's Upload Method selection.

## When this doesn't apply

- The **Nordic nRF52840 USB Dongle (PCA10059)** uses Nordic Secure DFU over USB
  CDC rather than the USB DFU class. Select its `Nordic PCA10059 USB serial DFU`
  layout; the image is linked at `0x1000`, package storage ends at `0xE0000`,
  and the serial-DFU packager uses no-SoftDevice requirement `0x00`.
- The official **nRF52840-DK** (with onboard J-Link OB) is flashed via SWD with an external probe (OpenOCD / pyOCD / `JLink.exe`).
- Generic `devboard_nrf52833` targets are packaged as SWD-first models rather than a USB-DFU workflow.
- For boards stuck in an unknown state, use the **manual double-reset** fallback: tap the reset button twice quickly to force the bootloader, which presents the DFU serial port, then upload normally.
