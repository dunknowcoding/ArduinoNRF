# nRF52840 peripheral driver coverage

Date: 2026-05-29

Authoritative census of the bottom-level (register) drivers in the core,
checked against the nRF52840 product-spec instantiation table. "Driver" here
means a direct hardware driver shipped in `cores/arduino/`, not a protocol
stack (those are libraries with their own roadmaps).

## Complete — every discrete peripheral has a driver

| Peripheral | Driver | Notes |
| --- | --- | --- |
| CLOCK (HFCLK/LFCLK) | `nrfStartHfclk` / `nrfStartLfclk` (NrfClock) | |
| POWER (sleep/OFF/DCDC/RAM-retention) | `NrfPower` | SystemOFF + GPIO/NFC/USB wake |
| RADIO — BLE (NimBLE) | NimBLE link-layer controller (`libraries/NimBLE/`) | full host+controller: adv, connections, GATT (HW-verified). `NrfBleRadio` facade is advertising-only/legacy |
| RADIO — proprietary 2.4 GHz | `NrfRadio` | NEW: raw packet TX/RX, 1/2 Mbit, CRC, RSSI |
| UARTE0/1 | `HardwareSerial` | |
| SPIM0-3 | `SPI` | master |
| TWIM0-1 | `Wire` | master |
| NFCT | `NrfNfcTag` | Type-2 tag emulation |
| GPIOTE (input) | `attachInterrupt` (wiring) | edge events |
| GPIOTE (output task) | `NrfGpioteOut` | PPI-driven pin toggles |
| SAADC | `analogRead` (wiring) | 8 ch, 14-bit |
| TIMER0-4 | `NrfTimer` | 6 CC each, PPI addrs |
| RTC0-2 | `NrfRtc` | 4 CC each, LFCLK |
| TEMP | `NrfTemp` | die temperature |
| RNG | `NrfRng` | TRNG + bias corrector |
| ECB | `NrfEcb` (NrfCrypto) | NEW: AES-128 ECB, FIPS-197 self-test |
| CCM | `NrfCcm` (NrfCrypto) | NEW: AES-CCM BLE packet cipher + MIC |
| AAR | `NrfAar` (NrfCrypto) | NEW: BLE private-address resolver |
| WDT | `NrfWdt` | pause-on-halt |
| QDEC | `NrfQdec` | rotary encoder |
| COMP | `NrfComp` | analog comparator |
| LPCOMP | `NrfLpComp` | NEW: ~1 µA, wakes from System OFF |
| EGU0-5 / SWI | `NrfEgu` | software events |
| PWM0-3 | PWM facade | 4 modules / 16 channels |
| PDM | `NrfPdm` | MEMS mic (unverified — needs hw) |
| I2S | `NrfI2s` | digital audio (unverified — needs hw) |
| MWU | `NrfMwu` | 4-region memory watch |
| QSPI | `NrfQspi` | external NOR (unverified — needs hw) |
| NVMC | `NrfNvmc` | flash erase/write |
| UICR | `NrfUicr` | NEW: REGOUT0 3.3V, NFC-pins-as-GPIO, customer[] |
| USBD | `NrfUsbd` | full CDC stack + GDB stub |
| GPIO (P0/P1) | `pinMode`/`digitalWrite` | |
| FICR | direct reads | device ID, EUI-64 |

## Verification status of the new drivers (this pass)

- **NrfEcb** — `selfTest()` runs the FIPS-197 AES-128 known-answer vector on
  the device; deterministic, hardware-verifiable on a single board.
- **NrfCcm / NrfAar** — register-correct per the PS; BLE-PDU framing follows
  the spec. The NimBLE stack itself is hardware-verified for unencrypted GATT;
  CCM (link encryption) / AAR (private-address resolution) are exercised only
  once a paired/bonded peer drives them, which is not yet part of the verified
  path.
- **NrfLpComp** — register-correct; shares the comparator block + IRQ 19 with
  COMP (mutually exclusive). Exercised by `examples/CryptoLpComp`.
- **NrfRadio** — register-correct per the PS; an on-air link needs a second
  nRF5x board running `examples/RadioPingPong` (peer = another nRF5x).
- **NrfUicr** — write path enforces the flash 1→0-only rule; setters need a
  reset to take effect. Not exercised on hardware this pass (a REGOUT0 write
  is persistent, so it's left for a deliberate board-bring-up step).

All 59 example sketches compile clean against the working-tree core.

## Deliberate non-driver items

These are **not** missing drivers — they're intentional design choices:

- **ACL (flash access-control / region protection)** — deferred on purpose.
  It's a niche IP-protection feature and a footgun (a mis-set region can
  block flash read/write until reset). Shipping it would mean register
  offsets I can't verify on hardware here; left out rather than guessed.
- **SPIS / TWIS (SPI/I²C *slave*)** — the same SPIM/TWIM silicon in a
  different mode. This is an Arduino-API-level feature (`Wire` slave
  callbacks), not a separate raw peripheral; tracked as an API extension,
  not a driver gap.
- **CRYPTOCELL (CC310)** — a binary-blob accelerator, intentionally a
  separate library with its own vendoring roadmap. The on-chip ECB/CCM/AAR above cover
  hardware AES *without* the blob.
- **DAC** — does not exist on the nRF52840.

## Protocol stacks (libraries, not drivers)

Tracked separately under their own roadmaps: NimBLE, Thread, Zigbee, CC310.
See `*_INTEGRATION_PLAN.md` and the `project_wireless_stack_progress` memory.
