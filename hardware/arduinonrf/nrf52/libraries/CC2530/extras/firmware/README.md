# cc2530_radio firmware

`cc2530_radio` turns a TI **CC2530** module into a UART-controlled IEEE 802.15.4
radio co-processor for the **CC2530** ArduinoNRF library. It is a small, custom
firmware built with the free **SDCC** compiler — *not* TI Z-Stack — which is why
it runs on cheap clone CC2530 modules that hang on stock Z-Stack (those modules
won't start the 32 MHz XOSC from Z-Stack's `SLEEPCMD` init; this firmware uses
`CLKCONCMD` and falls back to letting the radio start the XOSC itself).

What it does: 802.15.4 TX + RX on channels 11–26, promiscuous (sniffer) or
filtered, with RSSI/LQI reporting, driven over UART0 @115200 (8N1).

## Files
- `cc2530_radio.c` — the firmware source (SDCC / mcs51).
- `cc2530_radio.hex` — prebuilt Intel HEX (for TI SmartRF / a CC-Debugger).
- `cc2530_radio.bin` — prebuilt raw image (for CCLib-style flashers).
- `CC2530Flasher/` — flash it with **another ArduinoNRF board as the programmer**
  (no CC-Debugger needed).

## Wiring

**For flashing (debug interface)** — ArduinoNRF programmer board → CC2530:

| Programmer (ProMicro nRF52840) | CC2530 |
|---|---|
| D8  | P2.1 (DD, Debug Data) |
| D9  | P2.2 (DC, Debug Clock) |
| D10 | RST |
| 3V3 | VCC |
| GND | GND |

**For runtime (UART)** — host ArduinoNRF board → CC2530:

| Host (ProMicro nRF52840, Serial1) | CC2530 |
|---|---|
| D0 (TX) | P0.2 (RX) |
| D1 (RX) | P0.3 (TX) |
| 3V3 | VCC |
| GND | GND |
| — | **P2.0 (CFG1) → GND** |

## Flash it (board-as-programmer, recommended)
1. Wire the **debug** pins above.
2. Open `CC2530Flasher/CC2530Flasher.ino`, upload it to the programmer board.
3. Open the Serial Monitor @115200 — it prints `FLASH OK` when done.
4. Rewire for **runtime** (UART) and use the `CC2530` library examples.

## Flash it (TI SmartRF Flash Programmer + CC-Debugger)
Open `cc2530_radio.hex` in SmartRF Flash Programmer and program the CC2530.

## Build from source
Install [SDCC](https://sdcc.sourceforge.net/), then:
```
sdcc -mmcs51 cc2530_radio.c
makebin -p cc2530_radio.ihx cc2530_radio.bin   # or: objcopy -I ihex -O binary cc2530_radio.ihx cc2530_radio.bin
```

## UART protocol (for reference)
Frames: `FE  LEN  CMD/RESP  [DATA..]  FCS`  (LEN = 1+len(DATA); FCS = XOR of LEN..DATA).

Host → CC2530: `01` PING · `02` SET_CHANNEL[ch] · `03` TX[psdu..] · `04` SET_PROMISC[0|1]
CC2530 → host: `80` RESET_IND[ver] · `81` PONG[ver] · `82` OK · `83` TXDONE[status] · `84` RX_FRAME[rssi, crc|lqi, psdu..]
