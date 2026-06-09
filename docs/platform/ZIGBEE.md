# Zigbee / IEEE 802.15.4 on ArduinoNRF

ArduinoNRF does Zigbee / 802.15.4 through an **external radio module** driven over
UART — the working, recommended path today — plus a **built-in flasher** so you
need no external programmer.

## The working path: an external CC2530 module

- **Driver library:** **[ArduinoNRF-Zigbee](https://github.com/dunknowcoding/ArduinoNRF-Zigbee)**
  — install it separately in the Arduino IDE. It gives raw 802.15.4
  send / receive / promiscuous-sniff via the `CC2530Radio` API over `Serial1`,
  and bundles the SDCC transceiver firmware for the module.
- **Built-in flasher:** [`libraries/CCDebugger/`](../../hardware/arduinonrf/nrf52/libraries/CCDebugger/)
  turns the nRF52840 into a TI CC2530 programmer (bit-bang debug + DMA flash +
  read-back verify) — no external TI CC-Debugger required.

Hardware-verified on two AliExpress CC2530 clones: flash + read-back verify,
runtime PING over UART at 115200, and a two-node raw 802.15.4 link.

### Quick use
1. Install the **ArduinoNRF board package** (provides the board + `CCDebugger`)
   and the **ArduinoNRF-Zigbee** library.
2. Wire **DD/DC/RST → D8/D9/D10** (flashing) and **UART → D0/D1** (runtime), all
   at **3.3 V**. Full wiring is in the library's `docs/WIRING.md`.
3. Run *Examples ▸ ArduinoNRF-Zigbee ▸ **CC2530_FlashFirmware*** once.
4. Use *CC2530_Info / CC2530_Sniffer / CC2530_Link*.

### Notes & gotchas
- **3.3 V only** — the CC2530 is not 5 V tolerant.
- **nice!nano-style bootloader layout:** if `INFO_UF2.TXT` says
  `SoftDevice: not found`, select the no-SoftDevice bootloader option
  (`bootloader=promicroserialnosd`) so sketches are linked at `0x1000`. A
  SoftDevice layout (`0x26000` / `0x27000`) may upload successfully but the
  application will not run on that bootloader.
- **Serial1 reliability:** ArduinoNRF 0.3.3 fixes UARTE0 single-byte RX handling
  for short framed UART replies. NiusZigbee 0.1.2 also resynchronizes the
  CC2530 UART parser in `CC2530Radio.begin()` so uploads/resets do not leave the
  module mid-frame.
- **P2.0 (CFG1):** not used by the SDCC firmware (floating or grounded both work);
  ground it only if you later flash TI Z-Stack.
- This is **raw 802.15.4**, not full Zigbee PRO — ideal for custom links and
  sniffing; talking to real Zigbee devices needs a proper MAC header / a Zigbee
  stack.
- Many **clone** CC2530 modules won't boot stock TI Z-Stack (a crystal-startup
  quirk). The ArduinoNRF-Zigbee SDCC firmware starts the clock differently and
  runs on them.

### Verified CC2530_Link output

With the same sketch on two boards, both serial monitors should show periodic
transmits and receives:

```text
Link up on channel 11. Broadcasting every 2 s.
TX "hello 0" ok
RX (-47 dBm): hello 0
TX "hello 1" ok
RX (-44 dBm): hello 1
```

`CC2530_Link` enables promiscuous receive mode, so other 802.15.4 traffic on the
same channel can appear as non-printable or noisy frames. The `hello N` frames
from the other board are the link-health signal.

## nRF-native Zigbee (not vendored)

[`libraries/Zigbee/`](../../hardware/arduinonrf/nrf52/libraries/Zigbee/) exposes
an API for the nRF52840's **own** 802.15.4 radio, but the Zboss + nrf-802154
runtime is **not in-tree** — `begin()` returns `ZIGBEE_NOT_VENDORED` and
`isAvailable()` is `false`. Use the external CC2530 path above for working Zigbee
today.
