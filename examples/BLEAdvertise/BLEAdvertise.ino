// Applicable boards: packaged nRF52 variants.
// Limitations: validates the advertising-only facade; runtime advertising currently requires a declared low-frequency clock source.

#include <BLE.h>

static const BLEAdvConfig ADV = {
  "ArduinoNRF",
  "nRF52 Demo",
  0,
  160
};

static const unsigned long WAIT_MS = 1000;

void setup() {
  while (!BLE.begin(ADV)) {
    delay(WAIT_MS);
  }

  while (!BLE.adv()) {
    delay(WAIT_MS);
  }
}

void loop() {
  BLE.poll();
  delay(100);
}
