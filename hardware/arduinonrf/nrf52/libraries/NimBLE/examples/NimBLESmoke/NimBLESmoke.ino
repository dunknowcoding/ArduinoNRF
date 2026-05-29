// NimBLESmoke.ino - sanity-check NimBLE library wiring.
// Without vendored sources, prints NIMBLE_NOT_VENDORED. After M1 lands it
// prints NIMBLE_OK and the device's public BLE address.
#include <Arduino.h>
#include <NimBLE.h>

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000UL) {}
  Serial.println(F("NimBLE smoke"));
  const auto r = NimBLE::begin("ArduinoNRF-test");
  Serial.print(F("  begin: ")); Serial.println(r);
  Serial.print(F("  available: ")); Serial.println(NimBLE::isAvailable());
  if (NimBLE::isAvailable()) {
    uint8_t addr[6];
    NimBLE::getPublicAddress(addr);
    Serial.print(F("  addr: "));
    for (int i = 0; i < 6; ++i) { if (addr[i] < 0x10) Serial.print('0'); Serial.print(addr[i], HEX); if (i < 5) Serial.print(':'); }
    Serial.println();
    NimBLE::startAdvertising();
  }
}
void loop() { delay(1000); }
