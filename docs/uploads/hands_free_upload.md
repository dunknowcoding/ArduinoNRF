# Hands-free upload (1200-bps touch → serial DFU)

The default upload path for every Adafruit-fork board in this package. Plug a USB cable in, click **Upload**, and the new firmware is on the board — no button press, no manual bootloader entry.

## How it works

1. The Arduino IDE / `arduino-cli` opens the **service / maintenance CDC** at **1200 baud** and immediately closes it (this is the long-standing Arduino "1200-bps touch").
2. The firmware sees the touch and triggers `SYSRESETREQ` with the bootloader magic in `GPREGRET`, rebooting straight into the Adafruit serial-DFU bootloader.
3. The bootloader re-enumerates its CDC; `adafruit-nrfutil dfu serial` streams the packaged firmware over it.
4. The bootloader resets back to the new app, which re-enumerates on the same COM.

## Required board settings

`boards.txt` for the board family enables:

```text
upload.tool=niusdfu
upload.use_1200bps_touch=true       # IDE owns the touch (cross-platform)
upload.wait_for_upload_port=true    # IDE waits for the bootloader CDC
upload.bootloader_mode=adafruit-dfu # serial DFU via adafruit-nrfutil
```

The verified ProMicro path keeps `use_1200bps_touch=false` and lets `upload.ps1` own the touch instead — this gives it VID/PID-aware port remap and concurrency-safe retries on Windows where `usbser.sys` is touchy. Both designs are valid; the IDE-driven touch is what Adafruit's BSP uses and is the cross-platform default.

## Windows — `upload.ps1`

A hardened PowerShell pipeline drives the touch and DFU. Beyond the basics it provides:

- A per-port mutex so double-clicking **Upload** can't interleave two flashes.
- A bridge-yield IPC so an active debug session releases the COM for the upload.
- A wrong-port guard (selecting the user CDC is rejected with a clear message).
- A pre-touch PnP-snapshot cache (~3 s saved on the wall-clock).

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

See **[../COMPATIBILITY.md](../COMPATIBILITY.md)** for the full matrix. The path is verified on the **AliExpress ProMicro nRF52840** clone and is the *same* pipeline for all Adafruit-fork boards (nice!nano v2, SuperMini, XIAO, Pitaya Go, nRFMicro) once the corrected VID:PIDs are in place.

## When this doesn't apply

- The **Nordic nRF52840 USB Dongle (PCA10059)** uses Nordic Open DFU, not Adafruit serial DFU — flash with **nRF Connect for Desktop → Programmer** or `nrfutil`.
- The official **nRF52840-DK** (with onboard J-Link OB) is flashed via SWD — see **[swd_only_upload.md](swd_only_upload.md)**.
- For boards stuck in an unknown state, see **[double_reset_upload.md](double_reset_upload.md)** for the manual bootloader-entry fallback.
