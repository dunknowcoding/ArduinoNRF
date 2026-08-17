# SoftDevice handling on the ArduinoNRF core

## TL;DR

**This core is deliberately SoftDevice-free, and that is correct — not a missing
feature.** Every wireless stack in the package owns the radio directly, so the
core never calls `sd_softdevice_enable()`. Starting a Nordic SoftDevice would
seize `RADIO`, `TIMER0`, `RTC0`, the `CCM`/`AAR`/`ECB` crypto blocks and the
`CLOCK` arbitration, and would break all of those stacks at once.

What the core *does* now do (added 2026-06-15) is be **SoftDevice-aware and
safe**: it detects at boot whether a SoftDevice image is present in flash,
guarantees the application keeps full ownership of every peripheral, exposes the
SoftDevice's identity to sketches and tools, and offers a guarded opt-in hook
for the rare user who genuinely wants the certified Nordic BLE stack instead of
NimBLE/Thread.

## Why SoftDevice-free

| Stack | How it uses the radio | SoftDevice? |
|-------|-----------------------|-------------|
| **NimBLE** | Own link-layer controller, drives `RADIO` directly | No |
| **Thread / OpenThread** | Package's own bare-metal 802.15.4 `RADIO` driver (no nrfx) | No |
| **Zigbee** | External CC2530 transceiver over UART; nRF radio not involved | No |

A Nordic SoftDevice (e.g. S140) is a binary blob that takes exclusive ownership
of the radio and several timers/crypto blocks and exposes them only through
`svc` calls. It is incompatible with all three stacks above, which need raw
register access for their timing-critical protocols.

## Board reality

| Board | Flash layout | SoftDevice in flash |
|-------|--------------|---------------------|
| ProMicro-class nRF52840 clone with no-SoftDevice bootloader | app linked at `0x1000`, no SoftDevice region | **None** |
| nice!nano v2 with S140 v6 bootloader | app linked at `0x26000`, S140 v6.1.1 below it | **S140, dormant** |

On the ProMicro clones there is literally no SoftDevice to start — there is no
SoftDevice region in flash. On the nice!nano the S140 sits below the application
but is never enabled, so it claims nothing: the ECB crypto block and the radio
remain free (this is exactly why `NrfEcb` crypto works on the nice!nano).

## Detection

At reset, `Reset_Handler` calls `nrfSoftDeviceBootDetect()`. It reads the Nordic
SoftDevice information struct (magic `0x51B1E5DB` at SoftDevice-base + `0x2004`,
i.e. absolute `0x3004`) and stamps the result into a persistent diagnostic SRAM
slot so it can be read over SWD without a serial console:

| Address | Meaning |
|---------|---------|
| `0x2000400C` | `0x5DAB0000` = ran, no SoftDevice found · `0x5D00xxxx` = present, `xxxx` = firmware id |
| `0x20004010` | detected application start address (`0x1000` bare-metal, `0x26000` for S140) |

This sits next to the existing startup diag markers (`0x20004000`/`4`/`8`) inside
the SoftDevice-reserved SRAM zone (`0x20000000`–`0x20005FFF`) that neither the
bootloader nor our linker script touches, so it survives every reset.

### Verified over SWD

A ProMicro-class clone with no SoftDevice was tested. After flashing a current core build,
halting over SWD and reading `0x2000400C` returns `0x5DAB0000` and `0x20004010`
returns `0x00001000` — the core correctly detected "no SoftDevice, app at
`0x1000`" and proceeded bare-metal.

## API

`#include <NrfSoftDevice.h>` (core) or use `NordicHardware.softDeviceInfo()`.

```cpp
NrfSoftDevice::isPresent();          // SoftDevice image in flash?
NrfSoftDevice::status();             // Absent | Dormant | Enabled
NrfSoftDevice::appStartAddress();    // 0x1000 bare-metal, 0x26000 for S140
NrfSoftDevice::firmwareId();         // SD_FWID low 16 bits
NrfSoftDevice::versionRaw();         // major*1e6 + minor*1e3 + revision
NrfSoftDevice::ensureBareMetalReady();   // idempotent; refreshes the boot marker
```

```cpp
NordicSoftDeviceInfo sd = NordicHardware.softDeviceInfo();
// sd.present, sd.enabled, sd.baseAddress, sd.appStartAddress,
// sd.firmwareId, sd.versionRaw
```

See the `NordicHardware` library's **SoftDeviceInfo** example for a runnable
print-out.

## Safe coexistence guarantee

Because the core never enables a SoftDevice, every peripheral a SoftDevice would
reserve stays under application control from reset:

- `RADIO` — free for NimBLE / Thread
- `TIMER0`, `RTC0` — free
- `CCM`, `AAR`, `ECB` — free for `NrfCrypto` / AES
- `CLOCK` — no SoftDevice arbitration layer

The core additionally re-anchors `VTOR` to the application's own vector table on
every boot, so even a dormant MBR cannot route an interrupt into stale code.

## Opt-in: actually running a SoftDevice

This is **off by default and mutually exclusive with NimBLE and Thread**. If you
specifically want Nordic's certified BLE stack on a board that ships an S140
(nice!nano), you must:

1. Vendor the matching Nordic SDK / SoftDevice headers into your sketch.
2. Provide a strong `nrfSoftDeviceEnableImpl(bool)` that calls
   `sd_softdevice_enable()` / `sd_softdevice_disable()`.
3. Build with `-DNRF_ENABLE_SOFTDEVICE=1`.
4. Call `NrfSoftDevice::requestEnable()`.

Without all four, `requestEnable()` returns `false` and the core stays
bare-metal. This is intentional: it keeps the common path safe and makes the
NimBLE/Thread conflict an explicit, deliberate choice rather than an accident.

> Note: ProMicro clones with the no-SoftDevice bootloader cannot run a SoftDevice — there
> is no SoftDevice in their flash and the bootloader layout has the application
> at `0x1000`. The opt-in path only applies to S140-equipped boards.
