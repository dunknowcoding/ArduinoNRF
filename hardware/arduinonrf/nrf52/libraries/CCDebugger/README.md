# CCDebugger — ArduinoNRF's built-in TI CC2530 debugger & flasher

Turns an **ArduinoNRF (nRF52840)** board into a **CC2530/CC253x programmer** — no
external TI CC-Debugger or SmartRF programmer required. It bit-bangs the chip's
two-wire debug interface on three GPIOs and programs flash through the CC2530's
own DMA engine, then **reads it back and verifies**.

This is the nRF-side interface to Zigbee radio modules: use it to flash firmware
onto a CC2530, which the separate **ArduinoNRF-Zigbee** library then drives over
UART.

## Wiring (CC2530 ← ArduinoNRF, 3.3 V — NOT 5 V tolerant)

| CC2530 | ProMicro nRF52840 (default) |
|--------|------------------------------|
| P2.1 (DD, Debug Data)  | **D8**  |
| P2.2 (DC, Debug Clock) | **D9**  |
| RST                    | **D10** |
| VCC                    | 3V3 |
| GND                    | GND |

Any free GPIOs work — pass them to the constructor.

## Minimal usage

```cpp
#include <CCDebugger.h>
#include "my_firmware.h"          // const uint8_t FW[]; const unsigned int FW_LEN;

CCDebugger dbg(8, 9, 10);         // DD, DC, RST  (Arduino pin numbers)

void setup() {
  dbg.begin();
  dbg.enterDebug();
  if ((dbg.chipID() >> 8) == 0xA5) {        // 0xA5xx == CC2530
    dbg.flashFirmware(FW, FW_LEN);          // erase + program + verify
    dbg.run();                              // release debug, boot the chip
  }
}
void loop() {}
```

Ready-to-run examples:
- **CC2530_Identify** (this library) — read the chip ID, confirm the debug link.
- **CC2530_FlashFirmware** (ArduinoNRF-Zigbee library) — flash the radio firmware.

## API

| Method | Purpose |
|--------|---------|
| `CCDebugger(dd, dc, rst)` | construct with the DD/DC/RST Arduino pin numbers |
| `begin()` | resolve pins to fast GPIO (call once in `setup()`) |
| `enterDebug()` | reset the target into debug mode |
| `chipID()` | read chip id (`0xA5xx` = CC2530, `0x0000` = no response) |
| `status()` | read the debug status byte |
| `chipErase()` | mass-erase (also clears any debug lock) |
| `flashFirmware(data, len, progress=nullptr)` | erase + program an image starting at flash 0, verify by read-back checksum; optional `progress(percent)`; returns `true` if verified |
| `run()` | release DD/DC and reset the target so it runs its firmware |

A full 256 KB image (e.g. a TI Z-Stack build) takes ~90 s. When flashing a
Z-Stack `.hex`, convert to a flat binary and **exclude the last flash page**
(0x3F800–0x3FFFF, the lock bits) so you never set the debug-lock bit.

Hardware-verified on an AliExpress CC2530 module: chip-ID read, mass-erase, DMA
flash write, and read-back verify all confirmed.
