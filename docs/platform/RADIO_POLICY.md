# RADIO Resource Policy

nRF52840 has one 2.4 GHz RADIO. ArduinoNRF must make ownership explicit instead
of silently allowing incompatible stacks to fight each other.

## Runtime ownership matrix

| Stack/path | Uses nRF52840 RADIO | Runs inside Arduino sketch | Coexists with Arduino NimBLE |
|---|---:|---:|---:|
| Arduino NimBLE | Yes | Yes | N/A |
| NiusThread own-radio path | Yes | Yes | No |
| nCS Zigbee R23 sidecar | Yes | No, replaces sketch | No |
| Future in-sketch ZBOSS | Yes | Experimental future | No unless official MPSL path exists |
| External CC2530 | No | Yes | Yes |

## Current rule

- One onboard-radio stack at a time in the Arduino bare-metal runtime.
- External CC2530 is independent because it uses UART and its own radio.
- Official Zigbee R23 is implemented first as a sidecar firmware, not as a
  library linked into normal Arduino sketches.
- Multiprotocol work belongs in the nCS/MPSL path first.

## Debugging note

USB CDC and the USB GDB stub do not directly conflict with the RADIO, but
halting a CPU that is running a live wireless stack can break timing. Sidecar
Zigbee validation should prefer SWD/J-Link for early bring-up.
