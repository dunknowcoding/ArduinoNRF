// ZigbeeSmoke.ino - sanity-check Zigbee library wiring.
#include <Arduino.h>
#include <Zigbee.h>

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000UL) {}
  Serial.println(F("Zigbee smoke"));
  const auto r = Zigbee::begin(Zigbee::ROLE_END_DEVICE, /*PAN ID*/ 0x1234);
  Serial.print(F("  begin: ")); Serial.println(r);
  Serial.print(F("  available: ")); Serial.println(Zigbee::isAvailable());
  uint8_t eui[8]; Zigbee::getEui64(eui);
  Serial.print(F("  EUI-64: "));
  for (int i = 0; i < 8; ++i) { if (eui[i] < 0x10) Serial.print('0'); Serial.print(eui[i], HEX); }
  Serial.println();
}
void loop() { delay(1000); }
