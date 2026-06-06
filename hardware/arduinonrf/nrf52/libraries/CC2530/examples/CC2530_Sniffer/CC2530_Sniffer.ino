/*
  CC2530_Sniffer - Print every 802.15.4 frame heard on a channel.

  Turns a CC2530 module into a promiscuous 2.4 GHz sniffer. Each received frame
  is dumped as hex with its RSSI (dBm) and LQI. Open the Serial Monitor @115200.

  Wiring (CC2530 module -> ProMicro nRF52840, Serial1):
    P0.2 (RX) <- D0 (TX)     P0.3 (TX) -> D1 (RX)
    VCC <- 3V3   GND <-> GND   P2.0 (CFG1) -> GND
  Flash the cc2530_radio firmware first (see extras/firmware/README.md).
*/
#include <CC2530.h>

void onFrame(const uint8_t* psdu, uint8_t len, int8_t rssiDbm, uint8_t lqi) {
  Serial.print("[ch] len="); Serial.print(len);
  Serial.print(" rssi="); Serial.print(rssiDbm);
  Serial.print("dBm lqi="); Serial.print(lqi); Serial.print(" :");
  for (uint8_t i = 0; i < len; i++) {
    Serial.print(' ');
    if (psdu[i] < 0x10) Serial.print('0');
    Serial.print(psdu[i], HEX);
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  if (!CC2530.begin()) {
    Serial.println("CC2530 not responding - check wiring / firmware.");
  }
  CC2530.setChannel(15);        // 802.15.4 channels 11..26
  CC2530.setPromiscuous(true);  // receive every frame
  CC2530.onReceive(onFrame);
  Serial.println("CC2530 sniffer running on channel 15");
}

void loop() {
  CC2530.poll();                // dispatches frames to onFrame()
}
