# NimBLE integration plan — full BLE without the SoftDevice

## Scope and honest pacing

The current `libraries/BLE/` is a **570-line self-hosted advertising stub** that explicitly disclaims the rest: `gattServerSupported()`, `connectionsSupported()`, `notificationsSupported()`, and `otaDfuSupported()` all return `false`. The comment in `BLE.cpp` is candid — "the repository does not ship a board-agnostic SoftDevice." Real BLE (link layer + L2CAP + ATT + GATT + SMP + connection events + low-power) on the nRF52840 without Nordic's SoftDevice means **integrating an open-source BLE stack**.

The two realistic options are **Nordic's SoftDevice** (a closed binary blob — conflicts with the verified `promicroserialnosd` no-SoftDevice path) or **Apache Mynewt's NimBLE** (open source, vendored into the repo, ~20–50 K LOC). This document is the plan for the NimBLE route, which was explicitly chosen as the project direction.

**Bottom line on session pacing:** a clean NimBLE integration is genuinely **multi-session work** (vendoring + porting layer + RADIO/RNG/CCM wiring + Arduino glue + a first GATT example), each session producing one verifiable milestone. The plan below is sized in that unit.

## Milestones

Each milestone ends with a green build + a hardware sanity check.

### M1 — vendor + skeleton compile (1 session)

- Add a new top-level library `hardware/arduinonrf/nrf52/libraries/NimBLE/`.
- Vendor a known-good snapshot of `apache/mynewt-nimble` under `src/nimble/` (host, controller, transport, drivers/nrf5x).
- Provide a minimal porting layer (`porting/npl/` adapted from the freertos port but stripped down to bare-metal cooperative scheduling):
  - mutexes + semaphores collapsed into NVIC-level critical sections,
  - the event-queue / callout machinery driven from RTC0 (already exposed via this core's new `NrfRtc` — but NimBLE conventionally claims RTC0, so we'll route ours to RTC1 or RTC2 and document the conflict),
  - `os_get_uptime_usec()` backed by the same RTC.
- Provide a `library.properties` + `library.json` so `arduino-cli` discovers it.
- Goal: **the library + the existing `examples/BLEAdvertise` sketch (rewritten to call `NimBLE.begin()`) build clean.** No advertising yet — link-level only.

### M2 — controller comes up (1 session)

- Wire `nimble/drivers/nrf5x/` to the RADIO + RNG + CCM_AAR + TIMER0 (NimBLE's standard fast-path peripheral set).
- Add the IRQ handlers to the vector table (`RADIO_IRQHandler` is already a weak alias to `Default_Handler`; we override it).
- Pin HFCLK to the crystal so the link layer hits the BLE PHY timing window.
- Goal: **a fixed advertising payload is visible from `nRF Connect for Desktop` / `bluetoothctl` on the host PC.** Verified end-to-end on the J-Link-equipped ProMicro.

### M3 — GATT server + first connection (1 session)

- Implement the Arduino-shaped wrapper (`NimBLEService`, `NimBLECharacteristic`) on top of NimBLE's `ble_gatts_register` API.
- Echo characteristic example: write hex bytes from the central, read them back.
- Verify Pause/Resume, read/write, notify from the host PC.
- Update `examples/` with an `NimBLEEcho` sketch.

### M4 — security + sleep (1 session)

- Wire SMP for legacy + LE Secure Connections pairing using the nRF52's onboard ECB / RNG / DTLS.
- Add `lp_ticker` style sleep entry around the NimBLE event loop so the MCU drops to System ON between BLE events (the LFCLK-driven RTC keeps timing). Measure current draw via SWD-attached J-Link power monitoring (if exposed) or external multimeter.
- Verify a bonded connection survives a `__WFI()` sleep cycle.

### M5 — low/high-throughput stress (1 session)

- Set up a connection with min CI = 7.5 ms (high-speed) and max CI = 4 s (low-speed / power-sipping), measure throughput + power at each.
- Document the trade-offs.
- Land the final docs (`docs/platform/BLE_NIMBLE.md`).

## Conflicts to resolve up-front

- **RTC** — NimBLE's bare-metal port traditionally uses RTC0 for `os_callout` timing. This core's verified `promicroserialnosd` path leaves all three RTCs free; we will either route NimBLE to RTC1 (and document that the new `NrfRtc` driver should pick RTC2 by default) or carve out a thin shim so RTC0 is shared by event reference-counting. Decision in M1.
- **TIMER0** — NimBLE's controller wants TIMER0 dedicated to the radio scheduler. The core doesn't currently claim it.
- **PPI channels** — the controller takes a handful (typically 7–10). PPI is otherwise unused here.
- **Build geometry** — the linker reserves a flash region for the SoftDevice on `promicroserial` builds (the `with-BLE` variants). On `promicroserialnosd` the region is reclaimable; NimBLE's `.text` size (~50–80 KB depending on config) should fit comfortably in the ~800 KB user region.

## What the user can do today

Until M2 lands the existing `libraries/BLE/` advertising stub continues to work — it sets a `BLEService` + `BLECharacteristic` advertising payload (no connection support). Treat it as a name-and-payload broadcaster; a passive scan from the host PC will see the device name. Anything beyond passive observation needs the NimBLE integration above.
