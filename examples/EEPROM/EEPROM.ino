#include <EEPROM.h>

struct ConfigBlock {
  uint8_t marker;
  uint16_t version;
  uint16_t counter;
};

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  const ConfigBlock defaults = {0x5A, 0x0100, 0};
  ConfigBlock config = {};
  bool loaded = EEPROM.get(0, config, defaults, [](const ConfigBlock &value) {
    return value.marker == 0x5A;
  });

  if (!loaded) {
    Serial.println("EEPROM defaults loaded");
  }

  ++config.counter;
  EEPROM.put(0, config);
  while (!EEPROM.commit()) {
    delay(1000);
  }

  uint8_t marker = EEPROM.read(0);
  uint16_t version = 0;
  uint16_t counter = 0;
  EEPROM.get(1, version);
  EEPROM.get(3, counter);

  Serial.print("marker: 0x");
  Serial.println(marker, HEX);
  Serial.print("version: ");
  Serial.println(version);
  Serial.print("counter: ");
  Serial.println(counter);
}

void loop() {
  delay(20);
}
