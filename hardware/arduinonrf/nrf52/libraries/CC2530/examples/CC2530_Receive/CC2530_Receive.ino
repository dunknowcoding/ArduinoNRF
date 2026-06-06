/*
  CC2530_Receive - Receive 802.15.4 frames and print the payload as text.

  Pair with CC2530_Send. Shows how to use onReceive() to get frames with their
  RSSI/LQI. Open the Serial Monitor @115200.

  Wiring: see CC2530_Sniffer. Flash cc2530_radio first (extras/firmware/).
*/
#include <CC2530.h>

void onFrame(const uint8_t* psdu, uint8_t len, int8_t rssiDbm, uint8_t lqi) {
  // Skip the 7-byte MAC header (FCF+seq+PAN+addr) from CC2530_Send's frames.
  uint8_t hdr = (len >= 7) ? 7 : 0;
  Serial.print("RX ("); Serial.print(rssiDbm); Serial.print(" dBm): ");
  for (uint8_t i = hdr; i < len; i++) Serial.write(psdu[i]);
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  if (!CC2530.begin()) {
    Serial.println("CC2530 not responding - check wiring / firmware.");
  }
  CC2530.setChannel(15);
  CC2530.onReceive(onFrame);
  Serial.println("CC2530 receiver on channel 15");
}

void loop() {
  CC2530.poll();
}
