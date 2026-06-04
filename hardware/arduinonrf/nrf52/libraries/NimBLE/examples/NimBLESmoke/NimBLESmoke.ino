// NimBLESmoke.ino - sanity-check NimBLE library wiring.
// Without vendored sources, prints NIMBLE_NOT_VENDORED. After M1 lands it
// prints NIMBLE_OK and the device's public BLE address.
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
  g_beginResult = NimBLE::begin("ArduinoNRF-stdgatt") == NimBLE::NIMBLE_OK;
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
  // Drain the NimBLE host + controller event queues with low latency. This must
  // run continuously during a connection: a received CONNECT_IND/ATT request is
  // processed here, and the slave's response must be queued before the next
  // connection event. Keep blocking I/O (e.g. USB-CDC Serial writes) out of the
  // hot path - it stalls the host and makes the link unreliable.
  NimBLE::poll();
}
