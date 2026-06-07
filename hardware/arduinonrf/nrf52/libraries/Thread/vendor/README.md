# Vendoring OpenThread + nrf-802154 for nRF52840

Part of the in-progress OpenThread vendoring effort (not yet complete).

## Where to get it

```bash
# OpenThread (Google's reference impl, BSD-3)
git clone https://github.com/openthread/openthread.git --depth 1

# Nordic nrf-802154 PHY driver
git clone https://github.com/nrfconnect/sdk-nrfxlib.git
```

Copy these into `vendor/`:

| Source                                  | Destination                       |
|-----------------------------------------|-----------------------------------|
| `openthread/src/core/`                  | `vendor/src/openthread_core/`     |
| `openthread/src/include/openthread/`    | `vendor/include/openthread/`      |
| `openthread/src/cli/` (optional)        | `vendor/src/openthread_cli/`      |
| `sdk-nrfxlib/nrf_802154/`               | `vendor/src/nrf_802154/`          |

Pin OpenThread and nrf-802154 to versions that are known to interoperate
(see Nordic's nRF Connect SDK release notes for tested pairs).

After vendoring set `-DNRF_THREAD_VENDORED=1` in `boards.txt`.

## Conflicts

- **RADIO** — exclusive with NimBLE / Zigbee.
- **TIMER0** — claimed by `nrf-802154`.
- **PPI 0..7** — claimed.
- **RAM** — MTD ~12 KB, MED ~20 KB, SED ~8 KB.
- **Flash** — OpenThread MTD ~60–80 KB; adding Matter brings to ~200 KB.

See the integration plan for milestones M1..M6.
