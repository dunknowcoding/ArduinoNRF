// RadioPingPong.ino - proprietary 2.4 GHz radio link between two boards, plus
// a read of the UICR config. NO BLE / SoftDevice involved.
//
// Flash this on two ProMicro nRF52840 boards. Set ROLE_SENDER on one and
// comment it out on the other. The sender transmits an incrementing counter
// every 500 ms; the receiver prints each packet it hears with RSSI.
//
//   Sender : Serial prints "TX #N"
//   Receiver: Serial prints "RX #N  rssi=-XX dBm"
//
// Both boards must agree on channel + address + data rate (they do below).
//
// Also prints the UICR GPIO output-voltage setting at boot - handy to confirm
// whether a board has been bumped to 3.3V (see NrfUicr::setRegout0Voltage).

#include <NrfRadio.h>
#include <NrfPeripherals.h>   // NrfUicr

#define ROLE_SENDER   // <-- comment this out to build the receiver

static const uint8_t kAddress[5] = { 0xC2, 0xC2, 0xC2, 0xC2, 0x01 };
static const uint8_t kChannel    = 76;   // 2476 MHz - clear of common Wi-Fi

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  Serial.println(F("RadioPingPong"));

  // Report the GPIO high-drive output voltage from UICR.
  Serial.print(F("  UICR REGOUT0: "));
  switch (NrfUicr::regout0Voltage()) {
    case NrfUicr::V1_8: Serial.println(F("1.8V (default)")); break;
    case NrfUicr::V3_3: Serial.println(F("3.3V")); break;
    default:            Serial.println(F("other")); break;
  }

  if (!NrfRadio::begin(kChannel, NrfRadio::RATE_2MBIT, /*txdBm=*/4)) {
    Serial.println(F("  RADIO begin failed"));
    return;
  }
  NrfRadio::setAddress(kAddress);
  Serial.println(F("  RADIO up @ ch76, 2Mbit, +4 dBm"));
}

#ifdef ROLE_SENDER
void loop() {
  static uint32_t counter = 0;
  uint8_t pkt[4];
  pkt[0] = (uint8_t)(counter >> 0);
  pkt[1] = (uint8_t)(counter >> 8);
  pkt[2] = (uint8_t)(counter >> 16);
  pkt[3] = (uint8_t)(counter >> 24);
  if (NrfRadio::send(pkt, sizeof(pkt))) {
    Serial.print(F("TX #")); Serial.println(counter);
  } else {
    Serial.println(F("TX failed"));
  }
  ++counter;
  delay(500);
}
#else
void loop() {
  uint8_t buf[NrfRadio::MAX_PAYLOAD];
  uint8_t n = NrfRadio::receive(buf, sizeof(buf), /*timeoutMs=*/1000);
  if (n >= 4) {
    uint32_t counter = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
                       ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    Serial.print(F("RX #")); Serial.print(counter);
    Serial.print(F("  rssi=")); Serial.print(NrfRadio::lastRssiDbm());
    Serial.println(F(" dBm"));
  }
}
#endif
