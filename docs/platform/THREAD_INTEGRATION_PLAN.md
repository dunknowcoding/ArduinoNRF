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

### M1 — vendor public headers + library skeleton (DONE)

- Created `libraries/Thread/` with the same layout as NimBLE.
- Vendored `openthread/error.h`, `openthread/instance.h`, and **all 27**
  `openthread/platform/*.h` headers from `openthread/openthread@main`.
- Wrote `Thread.h` Arduino API (`begin(MTD/MED/SED)`, `joinNetwork`,
  `onCoapGet`, `getEui64`) returning `THREAD_NOT_VENDORED` until M3.
- Added `ThreadSmoke.ino` confirming the header tree links via
  `__has_include` probe.
- Status: shipped as part of commit `bef833c`. Compiles clean.

### M2 — platform abstraction with real backends (DONE)

- Wrote `platform_impl.cpp` implementing the OpenThread `otPlat*` contract
  the core will pull in at M3+.
- Real implementations (zero extra vendoring needed):
  - `otPlatAlarmMilli*` → RTC2 @ 1 kHz via `nrfRtc2()` (avoids RTC0
    SoftDevice clash; RTC1 reserved for NimBLE time-slice scenarios)
  - `otPlatAlarmMicro*` → TIMER3 @ 1 MHz via `nrfTimer3()`
  - `otPlatEntropyGet` → `NrfRng` hardware TRNG (M5 can swap to CC310 once
    vendored, per `CC310_INTEGRATION_PLAN.md`)
  - `otPlatRadioGetIeeeEui64` → `FICR.DEVICEID0/1` (factory-unique)
  - `otPlatReset` / `otPlatResetToBootloader` → AIRCR + GPREGRET (same
    path the verified hands-free upload uses)
  - `otPlatCAlloc` / `otPlatFree` → newlib `calloc`/`free`
- Stubs returning `OT_ERROR_NOT_IMPLEMENTED` (M3 targets): flash,
  settings, log.
- Status: shipped as commit `ef331c2`. ThreadSmoke compiles clean at
  74956 bytes (7%).

### M3 — vendor OpenThread core + nrf-802154 PHY (3–5 sessions, NOT 1)

**Scope reality check (discovered post-M2):**

- OpenThread core: **272 source files, 131,356 LOC** (`src/core/` from
  `openthread/openthread@main`). The MTD/FTD/RADIO build flavors are
  selected via CMake (`mtd.cmake` / `ftd.cmake` / `radio.cmake`); even
  MTD pulls in ~150 files. Heavily interconnected — partial vendoring
  produces a wall of link errors.
- nrf-802154 driver: 43 C + 54 H files (~32K LOC) under
  `nrfxlib/nrf_802154/`. Public headers depend on `hal/nrf_radio.h` and
  the rest of Nordic's `nrfx` HAL — that's another ~5K files
  foundational to all Nordic SDK code.

**Revised M3 plan:**

- **M3.a**: Vendor `nrfx` HAL subset (nRF52840-only headers + a handful
  of HAL drivers actually called by nrf-802154). Standalone smoke test.
- **M3.b**: Vendor `nrf-802154` driver. Provide platform shims
  (`nrf_802154_platform_clock_*`, `nrf_802154_irq_*`) bridged to
  `NrfClock` / NVIC. Smoke test: TX a beacon frame and confirm with a
  USB sniffer dongle.
- **M3.c**: Vendor OpenThread `src/core/common/`, `src/core/instance/`,
  `src/core/mac/`, `src/core/radio/`. Wire `otPlatRadio*` to
  `nrf-802154`. Goal: `otInstanceInitSingle()` succeeds.
- **M3.d**: Vendor `src/core/thread/`, `src/core/meshcop/`,
  `src/core/coap/`, `src/core/net/`. Goal: device pulls an IPv6 ULA
  from a Border Router (the original M2 goal in the old roadmap).

### M4 — was M2 / M3 in the original roadmap (1 session each, after M3)

- M4: MED + UDP echo + small `Thread.onCoapGet()` wrapper.
- M5: SED + sleep via `NrfPower::sleepMs()`. Target < 30 µA average.
- M6: Matter pairing.
- M7: Multi-protocol (Thread + BLE time-slice).

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
