# Thread (OpenThread) on ArduinoNRF

ArduinoNRF does Thread on the nRF52840's **own 2.4 GHz radio** — a full
OpenThread FTD stack vendored into the separate **NiusThread** library, running
on a register-level IEEE 802.15.4 driver written for this core. No SoftDevice,
no nrfx, no external radio module.

## The working path: the NiusThread library

- **Library:** **[ArduinoNRF-Thread (NiusThread)](https://github.com/dunknowcoding/ArduinoNRF-Thread)**
  — install it separately in the Arduino IDE. Four calls form or join a mesh
  (`Thread.begin() / setNetwork() / start() / process()`); the complete
  official OpenThread C API stays available via `Thread.instance()`.
- **In-package shim:** [`libraries/Thread/`](../../hardware/arduinonrf/nrf52/libraries/Thread/)
  keeps `#include <Thread.h>` building for existing sketches by forwarding to
  NiusThread.

Hardware-verified on two ProMicro nRF52840 boards: node 1 self-promotes to
**Leader**, node 2 attaches as **Child** and is promoted to **Router**;
bidirectional **UDP multicast** (`ff03::1`) delivers application data both
ways. MLE advertisements were also cross-checked over the air with a CC2530
sniffer during bring-up.

### Quick use

1. Install the **ArduinoNRF board package** and the **NiusThread** library.
2. Flash *Examples ▸ NiusThread ▸ **FormNetwork*** to two or more boards
   (same network name / channel / PAN id / key on all of them).
3. Watch the serial monitors: first board → `leader`, the rest → `child` /
   `router`.
4. Move on to *UdpBroadcast* for real data across the mesh, *CoapLamp* for
   CoAP resource control (the boards toggle each other's LEDs), and
   *NetworkInfo* for a live neighbor dashboard.

### Verified UdpBroadcast output

```text
UdpBroadcast: attaching...
role -> detached
role -> child
TX  "hello #1 from 0xE401"
RX  from fd00:db8:0:0:535d:5645:501c:5ef2  "hello #31 from 0xE400"
TX  "hello #32 from 0xC000"          <- promoted from child to router
RX  from fd00:db8:0:0:535d:5645:501c:5ef2  "hello #62 from 0xE400"
```

### Notes & gotchas

- **Peripheral conflicts:** the stack claims **RADIO** (exclusive with NimBLE /
  the proprietary `NrfRadio` / future own-radio Zigbee), **RTC2** (millisecond
  alarm) and **TIMER3** (microsecond alarm + timestamps). One sketch = BLE
  *or* Thread, not both.
- **nice!nano-style bootloader layout:** if `INFO_UF2.TXT` says
  `SoftDevice: not found`, build with a no-SoftDevice bootloader option
  (e.g. `bootloader=promicroserialnosd`) so the sketch links at `0x1000` — a
  SoftDevice layout uploads fine but never runs.
- **Footprint:** ~226 KB flash, ~32 KB RAM with the CoAP example.
- **Settings persist in flash**: the dataset, key sequence and frame counters
  live in two 4 KB pages directly below the bootloader (default `0xF2000`,
  override `NIUSTHREAD_SETTINGS_FLASH_BASE`), so nodes re-attach by themselves
  after a reboot. Still future work: CSL (sleepy children),
  commissioner/joiner and border-router roles.
- `Thread.process()` must run every `loop()` pass — the whole stack (alarms,
  radio events, tasklets) is polled from thread context.

## How it relates to the other 802.15.4 paths

| Path | Radio | Status |
| ---- | ----- | ------ |
| **Thread mesh** | nRF52840 own RADIO | ✅ working via **NiusThread** |
| **Raw 802.15.4 / sniffing** | external CC2530 over UART | ✅ working via **[ArduinoNRF-Zigbee](https://github.com/dunknowcoding/ArduinoNRF-Zigbee)** ([ZIGBEE.md](ZIGBEE.md)) |
| **Zigbee PRO (own radio)** | nRF52840 own RADIO | ⚠️ skeleton (`libraries/Zigbee/`, Zboss not vendored) |
