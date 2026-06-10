# Vendored OpenThread + mbedtls

Status: **vendored and hardware-verified** (single-node Leader formation;
MLE advertisements confirmed over the air by a CC2530 sniffer, 2026-06-10).

## What is vendored (in `src/`)

| Source                                   | Pin                | Destination in `src/`        |
|------------------------------------------|--------------------|------------------------------|
| `openthread/src/core/*` (15 subdirs)     | `fa3213ec`         | `api/ ... utils/` (flattened)|
| `openthread/src/include/common/*.hpp`    | `fa3213ec`         | `common/`                    |
| `openthread/include/openthread/`         | `fa3213ec`         | `openthread/`                |
| `mbedtls/include/{mbedtls,psa}`          | 3.6.5 (`e185d7fd`) | `mbedtls/`, `psa/`           |
| `mbedtls/library/*.{c,h}`                | 3.6.5 (`e185d7fd`) | `mbedtls_lib/`               |

The OT core tree is flattened to the `src/` root because the Arduino build
adds exactly one include root (`src/`) and the OT sources use
`"common/code_utils.hpp"`-style includes relative to their core root.

## Local patches (all marked `ARDUINONRF-PATCH`)

1. `src/openthread-core-config.h` - defaults
   `OPENTHREAD_PROJECT_CORE_CONFIG_FILE` to `"arduino-ot-config.h"`
   (Arduino cannot inject `-D` flags per library).
2. `src/mbedtls/mbedtls_config.h` - replaced with OpenThread's
   `third_party/mbedtls/mbedtls-config.h` so the OT crypto configuration is
   the library default (same reason).
3. `src/instance/extension_example.cpp` - deleted (vendor-extension template
   that upstream excludes from builds; Arduino compiles everything).

All build configuration lives in `src/arduino-ot-config.h` (FTD by default,
software MAC sub-layer, CSL off, TCP off, log -> Serial at NOTE level).

## Platform glue (ours, not vendored)

- `src/ot_radio_nrf52840.cpp` - register-level IEEE 802.15.4 RADIO driver
  (OT_RADIO_CAPS_NONE; IRQ RX ring, software address filter, imm-ack TX with
  source-match frame-pending, single hardware CCA per transmit).
- `src/platform_impl.cpp` - alarms (RTC2 ms / TIMER3 us, polled), TRNG
  entropy, RAM settings store (flash/NVMC backend is future work), logging,
  mbedtls static heap.
- `src/Thread.{h,cpp}` - the Arduino-facing API.

## Conflicts

- **RADIO** - exclusive with NimBLE / Zigbee / NrfRadio.
- **RTC2** + **TIMER3** - claimed by the alarm backends.
- **Flash/RAM** - FTD build: ~223 KB flash, ~35 KB static RAM.

## Updating

Re-run the copy table above against a newer openthread pin, re-apply the
three patches, and pin mbedtls to the submodule SHA recorded in
`openthread/.gitmodules` history (`git ls-tree <rev> third_party/mbedtls/repo`).
