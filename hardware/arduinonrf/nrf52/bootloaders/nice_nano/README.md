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
under `Tools > Programmer` (`SEGGER J-Link` or `CMSIS-DAP`). It performs a chip
recover/erase before programming the image.
