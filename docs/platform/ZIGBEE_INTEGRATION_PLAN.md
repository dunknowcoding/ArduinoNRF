# Zigbee integration plan — IEEE 802.15.4 + Zboss on nRF52840

## Scope & honest pacing

The nRF52840 has a 802.15.4-capable RADIO (the same one BLE uses, but switchable to 2.4 GHz 250 kbit/s O-QPSK in 802.15.4 mode). Putting a usable Zigbee stack on top of it is genuinely the largest single integration in this repo — substantially bigger than NimBLE — because Zigbee mandates **three layered protocols**, all needed for any real interop:

1. **PHY** — IEEE 802.15.4 OQPSK at 2.4 GHz. Nordic provides `nrf-802154` (open source).
2. **MAC + NWK** — Zigbee network layer with mesh routing, child management, key transport.
3. **APS + ZCL + ZDO** — Zigbee application support sub-layer, cluster library, device objects. **This is what app code talks to**.

The standard open implementation is **Nordic's Zboss** (BSD-licensed via the Nordic SDK, ~100–200 K LOC depending on profile). There is no realistic shortcut here.

**Pacing reality:** this is **6–10 focused sessions** in the same style as NimBLE M1–M5 was sized. Each milestone below is one session.

## Milestones

### M1 — vendor + skeleton compile (1 session)

- Create `libraries/Zigbee/`.
- Vendor `nrf-802154` from <https://github.com/nrfconnect/sdk-nrfxlib> (`nrfxlib/nrf_802154/`) and the Zboss stack from the Nordic nRF5 SDK (`external/zboss/`).
- Provide the Zboss platform abstraction layer for our bare-metal core (similar to NimBLE's NPL):
  - mutex / semaphore stubs collapsed to NVIC critical sections,
  - timer service backed by the new `NrfRtc` (route to RTC2 — RTC0 is "claimed by SoftDevice in spirit" and Zboss documentation expects to own a timer),
  - HFCLK request gate so the radio actually transmits.
- Goal: a `BareCoordinator.ino` sketch that calls `Zigbee.begin()` and `Zigbee.loop()` compiles and prints a TX-power-set log line. No air activity yet.

### M2 — PHY transmission + scan (1 session)

- Wire `nrf-802154` to RADIO + TIMER0 + PPI channels 0–7.
- Implement RAW frame TX + RX. Sniff with `Wireshark + nRF52 sniffer FW + nRF Connect for Desktop` to confirm we're transmitting on channel 11 (2.405 GHz).
- Add the IRQ handlers (`RADIO_IRQHandler` — currently weak alias to `Default_Handler`; we override). Document the RADIO + TIMER0 + PPI 0–7 claims so other libraries (NimBLE, custom apps) don't fight us.

### M3 — MAC association + key transport (1 session)

- Stand up Zboss as a **Coordinator** with PAN ID = some fixed value.
- Add a known-good Zigbee 3.0 router (a generic ZB router USB stick, or a smart bulb) and verify it associates: receive its Association Request, send Association Response, exchange Transport Key.
- Decrypt over-the-air traffic with Wireshark using the Trust Center Link Key (TCLK) to confirm the security layer is working.

### M4 — ZCL on/off cluster (1 session)

- Add minimal APS + ZCL support: send / receive On/Off cluster commands (cluster ID 0x0006).
- Example sketch: a coordinator that toggles a paired smart bulb every 5 seconds. This is the first **demoable** milestone.
- Goal: the example works against off-the-shelf Tuya / Ikea / Philips Hue Zigbee bulbs.

### M5 — End Device role + sleep (1 session)

- Switch the stack to act as an End Device that joins a Zigbee 3.0 coordinator and reports a temperature reading every 30 seconds.
- Wire the radio to enter sleep between events using `NrfPower::sleepMs()` and Zboss's `zb_sched_sleep`. Measure power draw — target < 50 μA average.
- Goal: a battery-powered sensor node that lives ~6 months on a CR2032.

### M6 — Touchlink, OTA, certification cleanup (1+ session)

- Add Touchlink commissioning (Zigbee 3.0 onboarding).
- Add OTA upgrade cluster — receive a new firmware image over Zigbee and flash it via the existing bootloader path.
- Run the Zigbee Alliance test suite (`ZCT`) against the implementation. Document any feature gaps versus full certification (which requires a paid testing lab anyway).

## Conflicts & costs

- **RADIO** — Zboss owns the RADIO peripheral exclusively. Cannot run alongside BLE (NimBLE) **unless** we implement multi-protocol time-slicing on top, which is a fourth-milestone item itself and out of scope for the basic plan. Treat Zigbee and BLE as mutually exclusive for v1.
- **TIMER0** — Zboss requires TIMER0 with sub-µs resolution. Sketches that already use TIMER0 must migrate to TIMER1–TIMER4.
- **PPI channels 0–7** — claimed exclusively. Sketches using these PPIs must move.
- **RAM footprint** — Zboss Coordinator typically uses ~30–40 KB RAM (routing tables, neighbor tables, security data). End-Device is much lower (~8 KB).
- **Flash footprint** — Zboss Coordinator binary is ~150–200 KB; End-Device profile ~80–100 KB. Fits comfortably in the ~800 KB user region for both.
- **Build complexity** — Zboss is delivered as ~50 .c files plus headers. The Arduino build system handles deep `src/` trees since IDE 1.5; we'll structure to fit.

## Source URLs

- IEEE 802.15.4 PHY/MAC driver `nrf-802154`: <https://github.com/nrfconnect/sdk-nrfxlib/tree/main/nrf_802154>
- Zboss (vendored via Nordic SDK): <https://github.com/nrfconnect/sdk-nrf/tree/main/subsys/zigbee>
- Zigbee Alliance certification info: <https://csa-iot.org/all-solutions/zigbee/>
- Nordic Zigbee getting-started docs: <https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/protocols/zigbee/index.html>
