# Vendoring NimBLE (Apache Mynewt) for nRF52840

NimBLE is the BLE stack we layer here to get connections + GATT + security
without the Nordic SoftDevice. This directory is where the NimBLE sources
land. Per the integration roadmap at
`docs/platform/NIMBLE_INTEGRATION_PLAN.md`, this is M1 work.

## Where to get it

```bash
git clone https://github.com/apache/mynewt-nimble.git --depth 1
```

The relevant subtrees to copy here:

| Source path                                                | Destination here                  |
|------------------------------------------------------------|-----------------------------------|
| `nimble/include/nimble/`                                   | `vendor/include/nimble/`          |
| `nimble/host/include/host/`                                | `vendor/include/host/`            |
| `nimble/host/src/`                                         | `vendor/src/host/`                |
| `nimble/host/services/`                                    | `vendor/src/services/`            |
| `nimble/controller/include/controller/`                    | `vendor/include/controller/`      |
| `nimble/controller/src/`                                   | `vendor/src/controller/`          |
| `nimble/drivers/nrf5x/`                                    | `vendor/src/drivers_nrf5x/`       |
| `nimble/transport/include/nimble/`                         | `vendor/include/nimble/`          |
| `nimble/transport/socket/src/`                             | `vendor/src/transport_socket/`    |
| `porting/nimble/include/nimble/`                           | `vendor/include/nimble/`          |
| `porting/npl/freertos/`                                    | `vendor/src/npl_freertos/`        |
| public headers promoted for Arduino compile                | `src/nimble/`, `src/host/`, `src/controller/` |

From this repository root you can stage that snapshot with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\\tools\\nimble\\sync_vendor.ps1 -Clean
```

The script does two things on purpose:

1. refreshes `vendor/` with the upstream staging snapshot
2. promotes the upstream public headers into `src/` so Arduino's library build can actually resolve `nimble/...`, `host/...`, and `controller/...` includes

It also writes `vendor/UPSTREAM_REVISION.txt` so future sessions can
see exactly which `drivers/mynewt-nimble` revision the staging area came from.

After vendoring set `-DNRF_NIMBLE_VENDORED=1` in
`hardware/arduinonrf/nrf52/boards.txt` (or via a build-flag menu) so
`src/NimBLE.cpp` switches from stub mode to real-hardware mode.

## Conflicts to know about

- **RADIO** — NimBLE controller owns it. Cannot run alongside Zigbee / Thread.
- **TIMER0** — claimed by `nimble/drivers/nrf5x/` for the link-layer scheduler.
- **PPI channels 0..7** — claimed by the controller.
- **RTC** — NimBLE's NPL uses RTC0 by default. The roadmap calls for routing
  it to RTC1 instead so the in-tree `NrfRtc` driver can keep RTC2 for users
  and the Nordic SoftDevice conventions are not broken.

See the integration plan for the full sized-out work.
