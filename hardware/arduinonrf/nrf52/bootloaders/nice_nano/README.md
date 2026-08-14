# nice!nano bootloader image

Bundled image:

- `nice_nano_bootloader-0.6.0_s140_6.1.1.hex`
- Source: <https://nicekeyboards.com/assets/nice_nano_bootloader-0.6.0_s140_6.1.1.hex>
- Referenced by the nice!nano troubleshooting guide:
  <https://nicekeyboards.com/docs/nice-nano/troubleshooting/>
- SHA-256:
  `2DF382129D5F8E3C852F2A9B4D8DAAE118D298BADE4A6DF9425F83F0B9B42F47`

This is a merged nice!nano / Adafruit nRF52 Bootloader image with Nordic S140
6.1.1. After burning it with SWD, sketches must use the S140 v6 layout
(`0x26000`) bootloader menu entries.

Use Arduino IDE `Tools > Burn Bootloader` only with an SWD programmer selected
under `Tools > Programmer` (`SEGGER J-Link` or `CMSIS-DAP`). It performs the
chip recover/erase step first and then programs the image. The bootloader HEX
itself does not disable `APPROTECT`; unlock comes from the SWD recover path.

## UF2 in-field bootloader update (no SoftDevice)

Bundled package:

- `update-nice_nano_bootloader-0.6.0_nosd.uf2` — Adafruit **update** UF2 (family
  `0xd663823c`). Rewrites MBR/bootloader/UICR for the 0x1000 no-SoftDevice layout.
  It is **not** a sketch and **not** the same as `Burn Bootloader` over SWD.

Deploy with scoped tooling (never copy to a fixed drive letter when multiple boards
are connected):

```powershell
.\hardware\arduinonrf\nrf52\tools\niusrobotlab\deploy_uf2_bootloader_update.ps1 -Port COMx
```

After the update the board **reboots into application mode**, not UF2. If the app
region at `0x1000` is empty or still built for SoftDevice (`0x26000`), USB may stay
off until you double-tap RESET and flash a matching no-SoftDevice sketch
(`bootloader=promicronosduf2` or `autonosd`). The deploy script can compile and
upload `MinimalUsbSmoke` automatically; use `-SkipRecoveryApp` to update only.

If USB is missing after update, double-tap RESET then re-run with
`-SkipBootloaderUpdate` to flash the recovery app only.

## Manual UF2 in DFU mode

Arduino IDE **Upload** (Windows) checks that the selected `Bootloader / DFU` app
start matches `INFO_UF2.TXT` before writing. **Dragging a `.uf2` in Explorer
does not.**

Before manual copy:

- Read `INFO_UF2.TXT` on the target drive. Match **layout** (`SoftDevice: not
  found` → app `@0x1000`; `SoftDevice: S140 …` → app `@0x26000`), not just
  `UF2 Bootloader 0.6.0`.
- Sketch UF2 must match that layout. Bootloader **update-*** packages change the
  bootloader only; follow with a matching app.

If COM and the UF2 drive disappear after a mismatched flash:

1. **Double-tap RESET** (two quick presses). No button: short **RST–GND** twice.
2. Re-enter UF2, then flash the correct sketch (IDE Upload or correct UF2).
3. Still silent: recover over **SWD** (J-Link or CMSIS-DAP).

See [../../../../docs/bootloaders/README.md](../../../../docs/bootloaders/README.md)
and [../../../../docs/platform/UPLOAD_BEHAVIOR.md](../../../../docs/platform/UPLOAD_BEHAVIOR.md).
