/*
  CCDebugger.h - ArduinoNRF's built-in TI CC2530/CC253x debugger & flasher.

  Lets an ArduinoNRF (nRF52840) board program a TI CC2530 (and compatible
  CC253x) Zigbee chip over its 2-wire debug interface - NO external TI
  CC-Debugger or SmartRF programmer required. This is the nRF-side interface to
  Zigbee radio modules: use it to flash the firmware that the separate
  ArduinoNRF-Zigbee library then drives over UART.

  It bit-bangs the CC debug protocol on three GPIOs and programs flash via the
  CC2530's own DMA engine (the proven method - reads back and verifies).

  WIRING (CC2530 module  <-  ArduinoNRF, 3.3 V):
      CC2530 P2.1 (DD , Debug Data )  <->  any GPIO  (default D8)
      CC2530 P2.2 (DC , Debug Clock)  <->  any GPIO  (default D9)
      CC2530 RST                      <->  any GPIO  (default D10)
      CC2530 VCC -> 3V3   GND -> GND
  (The runtime UART link P0.2/P0.3 <-> D0/D1 is separate and may stay connected.)

  USAGE:
      #include <CCDebugger.h>
      CCDebugger dbg(8, 9, 10);            // DD, DC, RST  (Arduino pin numbers)
      dbg.begin();
      if (dbg.enterDebug() && (dbg.chipID() >> 8) == 0xA5) {   // 0xA5xx = CC2530
        dbg.flashFirmware(FW, FW_LEN);     // FW[] = an image starting at flash 0
        dbg.run();                         // release debug, let the chip boot
      }

  See the CC2530_Flash example and the ArduinoNRF-Zigbee library's
  CC2530_FlashFirmware example for a complete, ready-to-run flasher. Programming
  a full 256 KB image takes ~90 s.
*/
#ifndef ARDUINONRF_CCDEBUGGER_H
#define ARDUINONRF_CCDEBUGGER_H

#include <Arduino.h>   // pulls in the nRF device headers (NRF_P0/NRF_P1, NRF_GPIO_Type)

class CCDebugger {
 public:
  /** @param ddPin/dcPin/rstPin Arduino pin numbers wired to DD / DC / RST. */
  CCDebugger(uint8_t ddPin, uint8_t dcPin, uint8_t rstPin);

  /** Resolve the pins to fast GPIO registers. Call once in setup(). */
  void begin();

  /** Reset the target and enter debug mode. @return true (always; check chipID). */
  bool enterDebug();

  /** Read the chip id word. CC2530 returns 0xA5xx. 0x0000 = not responding. */
  uint16_t chipID();

  /** Read the debug status byte. */
  uint8_t status();

  /** Mass-erase the chip (also clears any debug-lock). @return true on success. */
  bool chipErase();

  /**
   * Erase, then program @p data (an image that starts at flash address 0) and
   * verify it by read-back checksum.
   * @param data   firmware bytes (e.g. an SDCC .bin, or a Z-Stack image)
   * @param len    number of bytes (rounded up to whole 2 KB pages)
   * @param progress optional callback, invoked with 0..100 (percent) during write
   * @return true if the read-back checksum matches.
   */
  bool flashFirmware(const uint8_t* data, uint32_t len,
                     void (*progress)(uint8_t percent) = nullptr);

  /** Release the debug pins and reset the target so it runs its firmware. */
  void run();

 private:
  uint8_t ddPin_, dcPin_, rstPin_;
  uint32_t ddBase_, dcBase_, rstBase_;   // nRF GPIO port base (P0=0x50000000, P1=0x50000300)
  uint32_t ddBit_, dcBit_, rstBit_;
  bool ddIsOut_;

  // --- bit / byte level ---
  inline void dcHigh();
  inline void dcLow();
  inline void ddOut();
  inline void ddIn();
  inline void ddWrite(bool v);
  inline bool ddRead();
  void wr(uint8_t b);
  uint8_t rd();
  uint8_t switchRead();          // wait until target is ready
  // --- commands ---
  uint8_t cmd(uint8_t c);
  uint8_t exec1(uint8_t a);
  uint8_t exec2(uint8_t a, uint8_t b);
  uint8_t exec3(uint8_t a, uint8_t b, uint8_t c);
  uint8_t getReg(uint8_t r);
  void setReg(uint8_t r, uint8_t v);
  uint8_t getCfg();
  void setCfg(uint8_t v);
  void writeXdata(uint16_t a, const uint8_t* d, uint8_t n);
  void setDPTR(uint16_t a);
  void selXdataBank(uint8_t bank);
  void burst(const uint8_t* d, uint16_t n);
  // --- flash DMA ---
  void cfgDMA(uint8_t idx, uint16_t src, uint16_t dst, uint8_t trig,
              uint16_t len, uint8_t srcInc, uint8_t dstInc, uint8_t prio);
  void armDMA(uint8_t i);
  void disarmDMA(uint8_t i);
  bool dmaIRQ(uint8_t i);
  void clrDMAIRQ(uint8_t i);
  void faddr(uint16_t wordAddr);
  void clrFlashCtl();
  void setFlashWrite();
  void writePage(uint16_t pageNo, const uint8_t* data, uint16_t n);
  uint8_t readCode(uint8_t bank, uint16_t off);

  uint8_t page_[2048];
};

#endif  // ARDUINONRF_CCDEBUGGER_H
