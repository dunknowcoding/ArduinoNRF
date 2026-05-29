// PeripheralsDemo.ino - exercise NrfRng, NrfWdt, NrfTemp, NrfQdec on the
// onboard hardware. None of these needs CC310 / NimBLE / Zigbee / Thread.
//
//   * Reports last reset reason (so a WDT-induced reset is visible).
//   * Configures a 5-second watchdog; the main loop feeds it every 250 ms.
//   * Reads 8 random bytes from the hardware TRNG.
//   * Reads internal die temperature every 2 seconds.
//   * Optional rotary encoder on pins 2 / 3 - uncomment QDEC lines below.

#include <Arduino.h>
#include <NrfPeripherals.h>

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000UL) {}

  Serial.println(F("PeripheralsDemo"));

  // Was the last reset caused by our own watchdog?
  if (NrfWdt::causedLastReset()) {
    Serial.println(F("  ! last reset was from WDT - we lived through a watchdog"));
  }

  // 5-second WDT timeout, channel 0 enabled, pause-on-halt so debugging
  // doesn't reset the chip the moment you breakpoint.
  if (!NrfWdt::begin(5000UL, /*channelMask=*/1U, /*pauseOnHalt=*/true)) {
    Serial.println(F("  WDT begin failed"));
  } else {
    Serial.println(F("  WDT armed: 5 s timeout, feeding on channel 0"));
  }

  // TRNG: pull 8 random bytes.
  NrfRng::begin();
  uint8_t rnd[8];
  NrfRng::randomBytes(rnd, sizeof(rnd));
  Serial.print(F("  TRNG: "));
  for (uint8_t i = 0; i < sizeof(rnd); ++i) {
    if (rnd[i] < 0x10) Serial.print('0');
    Serial.print(rnd[i], HEX);
  }
  Serial.println();

  // Optional: rotary encoder on pins 2/3 (PIN_A = P0.02, PIN_B = P0.03 by
  // raw mapping). Uncomment to enable.
  // NrfQdec::begin(/*pinA=*/2, /*pinB=*/3);
}

void loop() {
  static uint32_t lastReport = 0;

  // Feed the watchdog on channel 0. As long as this fires within 5 s we
  // stay alive.
  NrfWdt::feed(0);

  // Print a snapshot every 2 seconds.
  const uint32_t now = millis();
  if (now - lastReport >= 2000UL) {
    lastReport = now;
    Serial.print(F("temp=")); Serial.print(NrfTemp::readCelsius(), 2);
    Serial.print(F(" C   uptime=")); Serial.print(now / 1000UL); Serial.print(F(" s"));
    // if (NrfQdec::isRunning()) {
    //   Serial.print(F("   encoder=")); Serial.print(NrfQdec::readPosition());
    // }
    Serial.println();
  }

  delay(250);   // includes yield() so USB stays responsive
}
