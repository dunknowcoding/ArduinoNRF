# ProMicro nRF52840 bootloader

This folder contains the ArduinoNRF recovery bootloader for the AliExpress
ProMicro nRF52840 board entry.

- `promicro_nrf52840_bootloader-0.9.2_s140_6.1.1.hex`
  - S140 v6 layout; application start is `0x26000`.
  - USB identity is `239A:00B3`.
  - Programs `UICR.PSELRESET[0/1]` to P0.18 so hardware reset and double-reset
    DFU keep working after a full J-Link recover / ERASEALL.
  - Leaves `UICR.APPROTECT` erased (`0xFF`) so SWD/J-Link recovery remains
    possible. Do not rebuild this image with `0x00` or `0x5A` at
    `0x10001208`; both are unsafe for this nRF52840 recovery path.
  - Programs `UICR.REGOUT0` for 3.3 V operation.
  - Falls through to USB UF2/CDC when no valid application is present. This is
    deliberate: a recovered blank board must enumerate over USB instead of
    silently entering BLE-only OTA.

- `promicro_nrf52840_bootloader-0.11.0_nosd.hex`
  - No-SoftDevice / MBR-only layout; application start is `0x1000`.
  - Built from the upstream Adafruit nRF52 bootloader `nice_nano` target
    because the verified clone-family USB identity is `239A:00B3`, volume
    `NICENANO`, model `nice!nano`.
  - Contains MBR + bootloader + UICR bootloader/MBR pointers only. It does not
    write `UICR.APPROTECT`, so a J-Link/CMSIS-DAP recover path remains open.
  - Select a no-SoftDevice `Bootloader / DFU` menu entry before using
    **Tools -> Burn Bootloader** with this image or before compiling sketches
    for a board recovered with this image.

- `update-promicro_nrf52840_bootloader-0.11.0_nosd.uf2`
  - UF2 bootloader update package for an already-working Adafruit/nice!nano
    class UF2 bootloader. It is not a sketch UF2.

Recovery check for the default S140 packaged image:

```powershell
nrfutil device fw-verify --serial-number <probe-serial> --traits jlink --family nrf52 `
  --swd-clock-frequency 50 `
  --firmware hardware\arduinonrf\nrf52\bootloaders\promicro_nrf52840\promicro_nrf52840_bootloader-0.9.2_s140_6.1.1.hex
```
