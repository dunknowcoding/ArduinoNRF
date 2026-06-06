/*
  CC2530.h - Drive a TI CC2530 module as an IEEE 802.15.4 radio co-processor.

  The CC2530 runs the NiusRobotLab "cc2530_radio" firmware (built with SDCC, see
  extras/firmware/) and is controlled from the nRF52840 over a UART using a small
  framed protocol. This lets an ArduinoNRF board add a second, independent
  2.4 GHz 802.15.4 radio - useful as a sniffer, a custom-protocol link, or to
  interoperate with other 802.15.4 / Zigbee devices.

  Wiring (default, Serial1 on the ProMicro nRF52840):
    nRF52840 TX (D0) -> CC2530 P0.2 (RX)
    nRF52840 RX (D1) -> CC2530 P0.3 (TX)
    GND <-> GND,  3V3 -> VCC,  CC2530 P2.0 (CFG1) -> GND

  The module first needs the firmware flashed once - see extras/firmware/README.md
  (board-as-programmer, no CC-Debugger required).

  Usage:
    #include <CC2530.h>
    void onRx(const uint8_t* psdu, uint8_t len, int8_t rssiDbm, uint8_t lqi) { ... }
    void setup() { CC2530.begin(); CC2530.setChannel(15); CC2530.onReceive(onRx); }
    void loop()  { CC2530.poll(); }
*/
#ifndef ARDUINONRF_CC2530_H
#define ARDUINONRF_CC2530_H

#include <Arduino.h>

class CC2530Class {
 public:
  /** Received-frame callback: 802.15.4 PSDU (no FCS), length, RSSI (dBm), LQI. */
  typedef void (*RxCallback)(const uint8_t* psdu, uint8_t len, int8_t rssiDbm,
                             uint8_t lqi);

  /** Largest 802.15.4 PSDU (127 bytes incl. the 2-byte FCS the radio appends). */
  static const uint8_t kMaxPsdu = 125;

  /**
   * Open the UART link and confirm the module answers. Defaults match the
   * shipped firmware (Serial1 @ 115200). Returns true if the module replies to
   * a PING.
   */
  bool begin(HardwareSerial& serial = Serial1, uint32_t baud = 115200);

  /** Round-trip PING. Optionally returns the firmware version (hi<<8 | lo). */
  bool ping(uint16_t* version = nullptr);

  /** Select the 802.15.4 channel (11..26). Returns true on ack. */
  bool setChannel(uint8_t channel);

  /** Promiscuous = receive every frame on the channel (sniffer). Default on. */
  bool setPromiscuous(bool on);

  /**
   * Transmit one 802.15.4 PSDU (you supply the MAC frame; the radio appends the
   * 2-byte FCS). Returns true if the radio reported TX complete.
   */
  bool send(const uint8_t* psdu, uint8_t len);

  /** Register the received-frame callback (invoked from poll()). */
  void onReceive(RxCallback cb) { rxcb_ = cb; }

  /** Pump incoming UART bytes and dispatch received frames. Call from loop(). */
  void poll();

 private:
  HardwareSerial* ser_ = nullptr;
  RxCallback rxcb_ = nullptr;

  // Frame parser state (FE | LEN | RESP | DATA.. | FCS)
  uint8_t buf_[160];
  uint8_t len_ = 0, idx_ = 0, state_ = 0;

  void sendFrame(uint8_t cmd, const uint8_t* data, uint8_t n);
  // Feed one byte; if a full valid frame completed, returns its RESP code and
  // fills resp/data/n, else returns 0.
  uint8_t feed(uint8_t b, uint8_t* outData, uint8_t* outN);
  void dispatchRx(const uint8_t* data, uint8_t n);
  // Read until a frame with RESP==want arrives or timeout; dispatches RX frames
  // seen meanwhile to the callback.
  bool waitResp(uint8_t want, uint8_t* outData, uint8_t* outN, uint32_t timeoutMs);
};

extern CC2530Class CC2530;

#endif  // ARDUINONRF_CC2530_H
