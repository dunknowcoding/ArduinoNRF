# Vendoring the Nordic CryptoCell 310 library

The CC310 hardware accelerator on the nRF52840 is driven by a closed-source
binary library from Nordic (`libcc_310.a` and matching headers). This
directory is where that binary lands.

## Where to get it

Download the **nRF5 SDK 17.x** from
<https://www.nordicsemi.com/Products/Development-software/nRF5-SDK> (free
registration required). After unpacking, the files you need are under
`external/nrf_cc310/`:

| Source path inside nRF5 SDK                                                   | Destination here                |
|-------------------------------------------------------------------------------|---------------------------------|
| `external/nrf_cc310/lib/cortex-m4/hard-float/no-interrupts/libcc_310_*.a`     | `vendor/lib/libcc_310.a`        |
| `external/nrf_cc310/include/*.h`                                              | `vendor/include/`               |
| `external/nrf_cc310_bl/lib/cortex-m4/hard-float/no-interrupts/libcc_310_bl_*.a` | `vendor/lib/libcc_310_bl.a` (optional, boot-loader subset) |

The **no-interrupts** flavor is the right one for bare-metal Arduino - the
freertos variant assumes Nordic's RTOS abstractions, which we don't have.

The exact filename varies by SDK version (`libcc_310_0.9.13.a`,
`libcc_310_0.9.16.a`, ...). Rename it to `libcc_310.a` after copying so the
build recipe finds it.

## After dropping the files

The `extra.libcc310.flags` placeholder in `boards.txt` adds
`-L vendor/lib -lcc_310 -I vendor/include` to the link line. The library's
src/NrfCC310.cpp probes for the presence of the linked symbols and switches
from stub mode to real-hardware mode automatically.

## License note

The Nordic CryptoCell 310 library is distributed under Nordic's binary
license (5-clause-BSD-style, but not OSI-approved). Read the LICENSE that
ships with the SDK before redistributing. This repository does NOT bundle
the binary - users must accept Nordic's terms and download themselves.
