# Thread integration plan — OpenThread on nRF52840

## Scope & honest pacing

Thread is an IPv6-based low-power mesh protocol from the Thread Group, built on the same IEEE 802.15.4 PHY as Zigbee but with a completely different network stack: 6LoWPAN + IPv6 + UDP/TCP, plus MeshCoP for commissioning. It's what Matter / Apple Home / Google Home use for low-power smart-home devices today, so this is the most strategically interesting protocol the nRF52840 supports.

The standard implementation is **OpenThread** (Google's open-source reference, BSD-3 license, ~50–80 K LOC). Nordic ships it via the nRF Connect SDK with their own `nrf-802154` PHY driver underneath. The Zboss Zigbee stack and OpenThread can share the same PHY driver — but only one stack at a time, since they expect exclusive control of the RADIO.

**Pacing reality:** ~5–7 focused sessions, sized like NimBLE M1–M5 was. OpenThread is smaller than Zboss so it's modestly less work than the Zigbee plan, but it's still genuine vendoring + porting effort.

## Why Thread (and not just Zigbee)

| | Zigbee | Thread |
|---|---|---|
| Protocol model | Zigbee-specific (APS / ZCL / ZDO) | Standard IPv6 + UDP/TCP + CoAP |
| Routing | Custom mesh | IETF-standard mesh (RPL-flavored) |
| Commissioning | Touchlink / EZ-Mode | MeshCoP + commissioning app |
| Smart-home gateways | Zigbee2MQTT / ZHA / Hue Bridge | **Matter / Apple Home / Google Home** native |
| Code footprint | Zboss Coordinator ~150–200 KB | OpenThread MTD ~60–80 KB |
| Sleep | End-device polling | Sleepy End Device (SED) — same idea, more standardized |
| Spec maturity | Zigbee 3.0 (2016) | Thread 1.3 (2022) + Matter |

For a new sketch in 2026: pick Thread if you want it to work with Apple Home / Google Home; pick Zigbee if you're targeting existing Zigbee2MQTT / SmartThings setups.

## Milestones

### M1 — vendor + skeleton compile (1 session)

- Create `libraries/Thread/` (matching the CC310 / NimBLE / Zigbee skeleton pattern in this repo).
- Vendor OpenThread from `openthread/openthread` and `nrf-802154` from `nrfconnect/sdk-nrfxlib`. The two repos are independent and tracked at their own release cadence; pin to a known-good combination (OpenThread 0.x.x + nrf-802154 from a matching nRFConnect SDK release).
- Provide the OpenThread platform abstraction (the `openthread/platform/` interface):
  - `otPlatRadio*` mapping to `nrf-802154`,
  - `otPlatAlarm*` mapping to `NrfRtc` (RTC2, to avoid clashing with NimBLE's RTC1 if both ever co-exist via time-slicing),
  - `otPlatEntropy*` mapping to `NrfRng` (hardware TRNG) or to CC310 once vendored,
  - `otPlatFlash*` mapping to NVMC / a reserved region for OpenThread settings.
- Goal: a `BareMtdSetup.ino` sketch that calls `Thread.begin()` + a no-op poll loop builds and links cleanly. No air activity.

### M2 — PHY + commissioning hand-off (1 session)

- Wire `nrf-802154` to RADIO + TIMER0 + PPI 0–7.
- Bring up OpenThread in Commissioner-handoff mode: print the device's EUI-64 over Serial, accept a commissioning passphrase, and use it to join an existing Thread network created by a Nordic / Apple / Google Border Router.
- Verify by watching the device come up in the Border Router's web UI with an IPv6 ULA.

### M3 — MED + UDP echo (1 session)

- Configure the device as a Minimal Thread Device (MED).
- Send a CoAP / UDP echo from a host on the Wi-Fi side of the Border Router and confirm it round-trips.
- Add a small Arduino-shaped CoAP server: `Thread.onCoapGet("/temp", []() { reply with internal temp });`. This is the first **demoable** milestone.

### M4 — SED + sleep (1 session)

- Switch to Sleepy End Device (SED) profile: poll the Parent Router every N seconds, sleep in between.
- Wire into `NrfPower::sleepMs()` so the CPU drops to System ON between polls. Measure: target < 30 µA average current on a CR2032.

### M5 — Matter pairing (1+ session)

- Layer Matter (CHIP) on top of OpenThread. Matter is its own 50-K-LOC stack; this milestone is large.
- Pair the device with Apple Home / Google Home using the standard Matter QR / setup code.
- Implement a Matter On/Off cluster so the device appears as a switchable light/outlet in the home app.

### M6 — Border Router co-existence + multi-protocol (optional)

- Time-slice the RADIO between Thread and BLE so a sketch can be **both** a Thread end-device **and** a BLE beacon. This is the multi-protocol use case Nordic ships in their SDK; it's possible but adds ~5–10 KB of glue code and careful timing.

## Conflicts & costs

- **RADIO** — exclusive with Zigbee, exclusive with BLE unless multi-protocol (M6). For v1, pick one.
- **TIMER0** — shared with Zigbee's `nrf-802154` requirement; same constraint as the Zigbee plan.
- **PPI channels 0–7** — exclusive; sketches using these must move.
- **RAM** — OpenThread MED needs ~20 KB RAM; MTD ~12 KB; SED ~8 KB. Comfortable on the 256-KB chip.
- **Flash** — OpenThread MTD ~60–80 KB; adding Matter brings the total to ~200 KB. Fits in the ~800-KB user region.

## Source URLs

- OpenThread: <https://github.com/openthread/openthread>
- Nordic `nrf-802154` PHY driver: <https://github.com/nrfconnect/sdk-nrfxlib/tree/main/nrf_802154>
- Thread spec downloads (free with registration): <https://www.threadgroup.org/Thread-1-3-0-Public-Specification>
- Matter spec (open via CSA): <https://csa-iot.org/all-solutions/matter/>
- Nordic OpenThread tutorials: <https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/protocols/thread/index.html>
