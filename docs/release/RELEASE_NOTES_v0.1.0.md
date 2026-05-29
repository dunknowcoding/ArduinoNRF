# ArduinoNRF v0.1.0

First minor-version release. Big additions across the board: a complete
multi-module PWM facade, RTC drivers, a full power-management surface, NFC-A
tag emulation, all the small nRF52840 peripherals (TRNG/Temp/WDT/QDEC/
TIMER/NVMC/PPI/EGU/COMP/MWU/GPIOTE-out), media-peripheral drivers (QSPI/
PDM/I2S), cross-platform Linux/macOS upload and debug paths, and corrected
real-board identities for every supported clone family. Plus skeleton
libraries + detailed integration roadmaps for the four big stacks
(NimBLE, CC310, Zigbee, Thread).

## Peripherals

* **PWM**: extended from 1 module / 4 channels to all 4 PWM modules / 16
  channels with independent frequency groups, per-pin polarity, and
  software-dead-time complementary pairing. New Tools → PWM speed menu
  selects the silent-default carrier (Auto / High-speed / Low-speed).
* **RTC**: new `NrfRtc` driver for RTC0/1/2 with 4 compare channels each
  and overflow IRQ. LFCLK-clocked so it survives System ON sleep.
* **Power**: full power-management API in `NrfPower.h`:
  System ON `sleep()` / `sleepWfe()` / `sleepMs()`, low-power vs constant-
  latency sub-modes, DCDC enable, RAM retention bitmap, **SystemOFF** with
  GPIO/NFC/USB wake source configuration, reset-reason diagnostics.
* **NFC-A Tag**: new `NrfNfcTag` driver for NFC Forum Type 2 tag emulation.
  Auto-collision-resolution, `beginUri(url)` / `beginText(text, lang)` /
  `beginRawNdef(ndef)`, factory-unique UID, read/field counters.
* **Small bottom-level drivers** (all in `NrfPeripherals.h`):
  - `NrfRng` — hardware TRNG with bias corrector
  - `NrfTemp` — internal die temperature sensor
  - `NrfWdt` — watchdog with pause-on-CPU-halt
  - `NrfQdec` — quadrature decoder for rotary encoders
  - `NrfTimer` — TIMER0..TIMER4 with 6 compare channels each
  - `NrfNvmc` — direct flash page-erase + word-write
  - `NrfPpi` — Programmable Peripheral Interconnect
  - `NrfEgu` — software event generator + SWI on 6×16 channels
  - `NrfComp` — analog comparator vs VDD\*fraction reference
  - `NrfMwu` — memory watch unit for buffer-overflow detection
  - `NrfGpioteOut` — GPIOTE output channels for PPI-driven pin transitions
* **Media peripherals** (in `NrfMediaPeripherals.h`, API complete but
  unverified — needs external hardware): `NrfQspi`, `NrfPdm`, `NrfI2s`.

## Cross-platform & compatibility

* **Linux + macOS** are now first-class targets for the upload + debug paths:
  `upload.py` drives `adafruit-nrfutil` cross-platform, and
  `usb_gdbstub_bridge.py` is a Python port of the Windows PowerShell
  bridge. Linux udev rules + setup instructions in
  `docs/COMPATIBILITY.md`.
* **Board identities corrected** against upstream sources. nice!nano /
  SuperMini / nRFMicro / XIAO / Pitaya Go USB VID:PIDs now match Adafruit /
  Seeed / Makerdiary / joric upstream; the nRF52840 USB Dongle is flagged
  as Nordic Open DFU (pipeline-incompatible); Mini and generic dev-board
  fall back to auto-detect.

## Multi-session work skeletons + roadmaps

Each comes with a vendor README listing the source repos / SDK paths to
drop in, plus a roadmap doc sized at NimBLE-style milestones:

* `libraries/NimBLE/` — Apache Mynewt NimBLE BLE stack
* `libraries/CC310/` — Nordic CryptoCell 310 binary crypto
* `libraries/Zigbee/` — Zboss + nrf-802154 Zigbee 3.0 stack
* `libraries/Thread/` — OpenThread + nrf-802154 Thread mesh (Matter target)

Roadmaps live under `docs/platform/`:

* `NIMBLE_INTEGRATION_PLAN.md`
* `CC310_INTEGRATION_PLAN.md`
* `ZIGBEE_INTEGRATION_PLAN.md`
* `THREAD_INTEGRATION_PLAN.md`

## Verified on hardware (AliExpress ProMicro nRF52840 clone, COM3 via J-Link)

* Hands-free upload, no manual reset, multi-COM-cycle.
* Single-USB-cable GDB-stub debug (breakpoints, step in/over/out,
  registers, memory, data watchpoints, pause / halt, peripheral SVD reads,
  concurrent user `Serial` on the second CDC).
* Dual-CDC concurrency (debug + user-Serial on the same cable).
* PWM multi-module allocation (caught m0 + m1 simultaneous via SWD).
* All 8 example sketches compile clean on the verified target.

## Upgrade notes

If you're upgrading from 0.0.1, no API-level breakage — all
existing `analogWrite` / `Serial` / `Wire` / `SPI` etc. surfaces are
backward-compatible. New optional features (multi-module PWM frequency,
per-pin polarity, complementary pairing) are additive. The `Tools →
PWM speed` menu defaults to "Auto" which behaves like 0.0.1.
