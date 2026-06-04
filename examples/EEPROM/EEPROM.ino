// EEPROM.ino - persist a small struct across resets using the emulated EEPROM.
//
// The nRF52840 has no real EEPROM, so this library emulates one in a flash
// page: reads/writes hit a RAM mirror, and commit() flushes that mirror to
// flash. A "boot counter" below proves the value survives a reset.

#include <EEPROM.h>

// The data we persist. Keep it small and plain (no pointers/objects).
struct ConfigBlock {
  uint8_t  marker;    // a "magic" byte: lets us tell saved data from blank flash
  uint16_t version;
  uint16_t counter;   // bumped once per boot
};

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  ConfigBlock config = {};
  const ConfigBlock defaults = {0x5A, 0x0100, 0};

  // Validated load. EEPROM.get(addr, out, defaults, isValid):
  //   1. reads a ConfigBlock from address 0 into `config`,
  //   2. calls the isValid() function below to check it,
  //   3. if invalid (e.g. first ever boot / blank flash), copies `defaults`
  //      into `config` and returns false.
  // The `[](const ConfigBlock &v) { ... }` is an inline (lambda) function -
  // just the validator passed in place.
  bool loaded = EEPROM.get(0, config, defaults, [](const ConfigBlock &v) {
    return v.marker == 0x5A;        // "does this stored block look valid?"
  });
  Serial.println(loaded ? "loaded saved config" : "first boot - using defaults");

  // Change a field and make it permanent.
  ++config.counter;
  EEPROM.put(0, config);            // stage the new bytes in the RAM mirror
  while (!EEPROM.commit()) {        // write the mirror to flash
    delay(1000);
  }

  Serial.print("boot count: ");
  Serial.println(config.counter);   // goes up by one on every reset
}

void loop() {
  delay(20);
}
