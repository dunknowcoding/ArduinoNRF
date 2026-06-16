# Official Nordic Stack Policy

ArduinoNRF's default runtime remains a lightweight Arduino bare-metal core, but
future official wireless stacks are integrated through the Nordic-supported
tooling that actually owns those stacks.

## Zigbee

| Stack line | Status | ArduinoNRF policy |
|---|---|---|
| nRF5 SDK for Thread and Zigbee | Deprecated historical path | Reference only |
| nCS Zigbee R22 | Maintenance/migration line | Do not start new work here |
| nCS Zigbee R23 add-on | Future-facing official Zigbee path | Primary onboard Zigbee target |

ArduinoNRF does not try to rebuild Zigbee R23 inside the Arduino builder. The
official Zigbee path is a sidecar nCS/Zephyr firmware flow with ArduinoNRF board
metadata, tooling, and validation layered around it.

## BLE

The Arduino sketch runtime uses the in-tree NimBLE host/controller port. It is
bare-metal and owns the RADIO directly.

SoftDevice remains a guarded advanced extension point. It is not required for
the Zigbee R23 sidecar path.

## Thread

The current Arduino path is the external NiusThread library. Future official
Thread or Matter work should follow the same rule as Zigbee: use nCS/Zephyr as
the official firmware sidecar first, then evaluate Arduino integration only
after the official path is reproducible.

## Version pinning

Official stack pins live in tracked metadata such as:

```text
hardware/arduinonrf/nrf52/tools/ncs_zigbee/pins.json
```

Large SDK downloads, west workspaces, and generated builds belong in:

```text
.ncs-zigbee-work/
```

That directory is ignored by Git and may be deleted at any time.
