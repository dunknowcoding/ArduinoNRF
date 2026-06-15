# Hands-free upload (1200-bps touch → UF2 / serial DFU)

The default upload path for every Adafruit-fork board in this package. Plug a USB cable in, click **Upload**, and the new firmware is on the board — no button press, no manual bootloader entry.

On Windows, `upload.ps1` now handles both UF2 mass-storage bootloaders and Adafruit serial DFU. With **Bootloader / DFU → Auto-detect**, a mounted UF2 drive for the selected board is preferred; explicit serial-DFU menu entries still force the `adafruit-nrfutil` path.

## How it works

1. The Arduino IDE / `arduino-cli` opens the **service / maintenance CDC** at **1200 baud** and immediately closes it (this is the long-standing Arduino "1200-bps touch").
2. The firmware sees the touch and triggers `SYSRESETREQ` with the bootloader magic in `GPREGRET`, rebooting straight into the bootloader.
3. If a matching UF2 drive is present, the wrapper converts the sketch HEX to UF2 and copies it to that drive. If an explicit serial-DFU bootloader menu is selected, `adafruit-nrfutil dfu serial` streams the packaged firmware over the bootloader CDC instead.
4. The bootloader resets back to the new app.

## Required board settings

`boards.txt` for the board family enables:

```text
upload.tool=niusdfu
upload.use_1200bps_touch=true       # IDE owns the touch (cross-platform)
upload.wait_for_upload_port=true    # IDE waits for the bootloader CDC
upload.bootloader_mode=auto         # Windows auto-detects UF2 vs serial DFU
```

The verified ProMicro path keeps `use_1200bps_touch=false` and lets `upload.ps1` own the touch instead — this gives it VID/PID-aware port remap and concurrency-safe retries on Windows where `usbser.sys` is touchy. Both designs are valid; the IDE-driven touch is what Adafruit's BSP uses and is the cross-platform default.

## Windows — `upload.ps1`

A hardened PowerShell pipeline drives the touch and DFU. Beyond the basics it provides:

- A per-port mutex so double-clicking **Upload** can't interleave two flashes.
- A bridge-yield IPC so an active debug session releases the COM for the upload.
- A wrong-port guard (selecting the user CDC is rejected with a clear message).
- A stale-port guard: if the selected COM disappeared after a bootloader transition, the upload fails clearly instead of matching another board.
- Stable-ID UF2 matching so two boards with the same volume label (for example two `NICENANO` drives) do not conflict.
- **Upload Method → Enter UF2 drive only (no upload)**, which leaves the selected board mounted as a UF2 drive and stops before copying firmware.
- A pre-touch PnP-snapshot cache (~3 s saved on the wall-clock).
- A **layout guard** on Windows: before UF2 or serial-DFU transfer, the wrapper
  reads `INFO_UF2.TXT` on the selected board's UF2 drive and rejects uploads when
  the compiled app start does not match the mounted bootloader layout.
- A **misflash guard** after same-PID serial DFU when USB serial never returns.

Manual UF2 drag-and-drop is **not** guarded; see
**[docs/platform/UPLOAD_BEHAVIOR.md](../platform/UPLOAD_BEHAVIOR.md)** and
**[docs/bootloaders/README.md](../bootloaders/README.md)**.

Driven by `tools.niusdfu.upload.pattern.windows` in `platform.txt`.

## Linux / macOS — `upload.py`

A small stdlib-only Python script that mirrors the proven Adafruit cross-platform recipe:

```text
adafruit-nrfutil dfu genpkg --dev-type 0x0052 --sd-req ... --application app.hex pkg.zip
adafruit-nrfutil dfu serial -pkg pkg.zip -p {port} -b 115200 --singlebank -t 1200
```

Driven by `tools.niusdfu.upload.pattern.linux` / `.macosx`. Requires `adafruit-nrfutil` on `PATH`:

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

- The **Nordic nRF52840 USB Dongle (PCA10059)** uses Nordic Open DFU, not Adafruit serial DFU — flash with **nRF Connect for Desktop → Programmer** or `nrfutil`.
- The official **nRF52840-DK** (with onboard J-Link OB) is flashed via SWD with an external probe (OpenOCD / pyOCD / `JLink.exe`).
- Generic `devboard_nrf52833` targets are packaged as SWD-first models rather than a USB-DFU workflow.
- For boards stuck in an unknown state, use the **manual double-reset** fallback: tap the reset button twice quickly to force the bootloader, which presents the DFU serial port, then upload normally.
