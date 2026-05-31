// NimBLESmoke.ino - sanity-check NimBLE library wiring.
// Without vendored sources, prints NIMBLE_NOT_VENDORED. After M1 lands it
// prints NIMBLE_OK and the device's public BLE address.
#include <Arduino.h>
#include <NimBLE.h>

namespace {
bool g_beginResult = false;
bool g_startedAdvertising = false;
uint8_t g_addr[6] = {0};
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000UL) {}
  Serial.println(F("NimBLE smoke"));
  g_beginResult = NimBLE::begin("ArduinoNRF-test") == NimBLE::NIMBLE_OK;
  Serial.print(F("  begin: ")); Serial.println(g_beginResult ? F("OK") : F("ERR"));
  Serial.print(F("  available: ")); Serial.println(NimBLE::isAvailable());
  if (NimBLE::isAvailable()) {
    NimBLE::getPublicAddress(g_addr);
    Serial.print(F("  addr: "));
    for (int i = 0; i < 6; ++i) { if (g_addr[i] < 0x10) Serial.print('0'); Serial.print(g_addr[i], HEX); if (i < 5) Serial.print(':'); }
    Serial.println();
    const auto advertisingStatus = NimBLE::startAdvertising();
    g_startedAdvertising = advertisingStatus == NimBLE::NIMBLE_OK;
    Serial.print(F("  startAdvertising: ")); Serial.println(static_cast<int>(advertisingStatus));
  }
}

void loop() {
  NimBLE::poll();
  Serial.print(F("tick ms=")); Serial.print(millis());
  Serial.print(F(" begin=")); Serial.print(g_beginResult);
  Serial.print(F(" available=")); Serial.print(NimBLE::isAvailable());
  Serial.print(F(" advertising=")); Serial.print(g_startedAdvertising);
  Serial.print(F(" addr="));
  for (int i = 0; i < 6; ++i) {
    if (g_addr[i] < 0x10) {
      Serial.print('0');
    }
    Serial.print(g_addr[i], HEX);
    if (i < 5) {
      Serial.print(':');
    }
  }
  Serial.println();
  // Pump NimBLE continuously for ~1s (NOT delay()): the LL event queue must be
  // drained promptly or a received CONNECT_IND is processed too late and the
  // connection anchor is missed. Status print stays ~1 Hz.
  uint32_t t = millis();
  while (millis() - t < 1000UL) {
    NimBLE::poll();
  }
}
