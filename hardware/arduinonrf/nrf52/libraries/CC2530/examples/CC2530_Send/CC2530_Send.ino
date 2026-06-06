/*
  CC2530_Send - Transmit an 802.15.4 broadcast frame once a second.

  Pair with CC2530_Sniffer (or any 802.15.4 receiver) on the same channel to see
  the frames. Open the Serial Monitor @115200.

  Wiring: see CC2530_Sniffer. Flash cc2530_radio first (extras/firmware/).
*/
#include <CC2530.h>

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  if (!CC2530.begin()) {
    Serial.println("CC2530 not responding - check wiring / firmware.");
  }
  CC2530.setChannel(15);
  Serial.println("CC2530 sender on channel 15");
}

void loop() {
  static uint8_t seq = 0;
  // A minimal 802.15.4 MAC data frame (FCS is appended by the radio):
  //   FCF(2) seq(1) destPAN(2) destAddr(2) payload
  uint8_t frame[] = { 0x41, 0x88, seq++, 0xFF, 0xFF, 0xFF, 0xFF, 'H', 'i' };
  bool ok = CC2530.send(frame, sizeof(frame));
  Serial.print("TX seq="); Serial.print(seq - 1);
  Serial.println(ok ? " ok" : " FAIL");
  delay(1000);
}
