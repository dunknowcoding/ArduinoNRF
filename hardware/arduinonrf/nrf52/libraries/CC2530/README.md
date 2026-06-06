# CC2530 — IEEE 802.15.4 radio co-processor for ArduinoNRF

Drive an external **TI CC2530** module as a second, independent 2.4 GHz
802.15.4 radio from your ArduinoNRF board, over a simple UART link. Useful as a
**sniffer**, a **custom 802.15.4 link**, or to interoperate with other
802.15.4 / Zigbee devices — alongside the nRF52840's own radio.

The CC2530 runs a small custom firmware (`cc2530_radio`, built with the free
**SDCC** compiler — see [`extras/firmware/`](extras/firmware/)). It deliberately
does **not** use TI Z-Stack, so it works even on cheap clone CC2530 modules that
hang on stock Z-Stack.

## Quick start
1. **Flash the module** once — open
   [`extras/firmware/CC2530Flasher`](extras/firmware/CC2530Flasher/) on a spare
   ArduinoNRF board wired to the CC2530's debug pins (no CC-Debugger needed), or
   program `extras/firmware/cc2530_radio.hex` with TI SmartRF. See
   [`extras/firmware/README.md`](extras/firmware/README.md).
2. **Wire the UART** (host board ↔ CC2530):

   | Host (ProMicro nRF52840, Serial1) | CC2530 |
   |---|---|
   | D0 (TX) | P0.2 (RX) |
   | D1 (RX) | P0.3 (TX) |
   | 3V3 | VCC |
   | GND | GND |
   | — | **P2.0 (CFG1) → GND** |

3. **Use the library:**
   ```cpp
   #include <CC2530.h>
   void onRx(const uint8_t* psdu, uint8_t len, int8_t rssiDbm, uint8_t lqi) { /* ... */ }
   void setup() {
     CC2530.begin();            // Serial1 @115200
     CC2530.setChannel(15);     // 11..26
     CC2530.onReceive(onRx);
   }
   void loop() { CC2530.poll(); }
   ```

## API
- `bool begin(HardwareSerial& = Serial1, uint32_t baud = 115200)` — open link, confirm the module (PING).
- `bool ping(uint16_t* version = nullptr)` — round-trip check; optional firmware version.
- `bool setChannel(uint8_t ch)` — 802.15.4 channel 11..26.
- `bool setPromiscuous(bool on)` — receive all frames (sniffer) vs filtered.
- `bool send(const uint8_t* psdu, uint8_t len)` — transmit a MAC PSDU (radio appends FCS); returns TX result.
- `void onReceive(RxCallback)` — `cb(psdu, len, rssiDbm, lqi)` for each received frame.
- `void poll()` — call from `loop()` to dispatch received frames.

## Examples
- **CC2530_Sniffer** — hex-dump every frame on a channel, with RSSI/LQI.
- **CC2530_Send** — transmit a broadcast frame once a second.
- **CC2530_Receive** — receive frames and print the payload.

## Notes
- Hardware-verified on an AliExpress CC2530 clone: PING/version, SET_CHANNEL, TX, and RX all working at 115200.
- Over SPI the CC2530's magnetometer-style features are N/A; this is a pure 802.15.4 radio.
- For full **Zigbee** (ZDO/APS/ZCL) on the CC2530 you still need TI Z-Stack (IAR build) or a known-good module; this library provides the 802.15.4 PHY/MAC layer.
