#include <EEPROM.h>

struct CompatBlock {
  uint8_t marker;
  uint16_t count;
};

void printYesNo(bool value) {
  if (value) {
    Serial.println("yes");
  } else {
    Serial.println("no");
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  while (!EEPROM.begin()) {
    delay(1000);
  }

  const CompatBlock defaults = {0xA5, 0};
  CompatBlock block = {};
  bool loaded = EEPROM.get(0, block, defaults, [](const CompatBlock &value) {
    return value.marker == 0xA5;
  });

  ++block.count;
  EEPROM.put(0, block);
  EEPROM[sizeof(CompatBlock)] = 0x5A;
  uint8_t echoed = EEPROM[sizeof(CompatBlock)];
  EEPROM[sizeof(CompatBlock)].update(0x5A);
  while (!EEPROM.commit()) {
    delay(1000);
  }

  CompatBlock verify = {0x00, 0};
  EEPROM.get(0, verify);

  Serial.print("loaded existing: ");
  printYesNo(loaded);
  Serial.print("marker: 0x");
  Serial.println(verify.marker, HEX);
  Serial.print("count: ");
  Serial.println(verify.count);
  Serial.print("echoed: 0x");
  Serial.println(echoed, HEX);

  EEPROM.end();
}

void loop() {
  delay(20);
}