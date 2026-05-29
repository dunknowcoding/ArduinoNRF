# Vendoring Zigbee (Zboss + nrf-802154) for nRF52840

Per the M1 milestone in `docs/platform/ZIGBEE_INTEGRATION_PLAN.md`.

## Where to get it

```bash
# nrf-802154 PHY driver (BSD-3)
git clone https://github.com/nrfconnect/sdk-nrfxlib.git
cp -r sdk-nrfxlib/nrf_802154/ vendor/src/nrf_802154/

# Zboss Zigbee stack (Nordic redistribution, BSD-style)
# Download nRF Connect SDK ("nrf") and copy from:
#   nrf/subsys/zigbee/                  -> vendor/src/zboss_subsys/
#   nrf/external/zboss/zboss_release/   -> vendor/src/zboss/
```

Pin the nRF Connect SDK version (e.g. 2.6.x) so the Zboss + nrf-802154
combination is known-good.

After vendoring set `-DNRF_ZIGBEE_VENDORED=1` in `boards.txt` to switch
the implementation from stubs to real hardware.

## Conflicts

- **RADIO** — exclusive with NimBLE / Thread (only one stack at a time).
- **TIMER0** — claimed by `nrf-802154`.
- **PPI 0..7** — claimed by `nrf-802154`.
- **RAM** — Coordinator ~30 KB, End Device ~8 KB.
- **Flash** — Coordinator ~150–200 KB, End Device ~80–100 KB.

See the full integration plan for milestones M1..M6.
