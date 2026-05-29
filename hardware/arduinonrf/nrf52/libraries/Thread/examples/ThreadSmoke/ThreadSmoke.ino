// ThreadSmoke.ino - sanity-check Thread library wiring.
#include <Arduino.h>
#include <Thread.h>

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000UL) {}
  Serial.println(F("Thread smoke"));
  const auto r = Thread::begin(Thread::ROLE_MED);
  Serial.print(F("  begin: ")); Serial.println(r);
  Serial.print(F("  available: ")); Serial.println(Thread::isAvailable());
  uint8_t eui[8]; Thread::getEui64(eui);
  Serial.print(F("  EUI-64: "));
  for (int i = 0; i < 8; ++i) { if (eui[i] < 0x10) Serial.print('0'); Serial.print(eui[i], HEX); }
  Serial.println();
}
void loop() { delay(1000); }
