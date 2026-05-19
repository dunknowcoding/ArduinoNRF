#include <EEPROM.h>

static uint8_t patternByte(int index) {
  return static_cast<uint8_t>((index * 37 + 11) & 0xFF);
}

void setup() {
  for (int index = 0; index < EEPROM.length(); ++index) {
    EEPROM.write(index, patternByte(index));
  }
  EEPROM.commit();

  for (int index = 0; index < EEPROM.length(); ++index) {
    if (EEPROM.read(index) != patternByte(index)) {
      while (true) {
        delay(1000);
      }
    }
  }
}

void loop() {
  delay(50);
}