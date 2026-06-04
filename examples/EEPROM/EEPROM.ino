// EEPROM.ino - store a value that survives resets, using the emulated EEPROM.
//
// The nRF52840 has no real EEPROM, so this library keeps the data in RAM and
// writes it to a flash page only when you call commit(). The familiar
// EEPROM.read()/EEPROM.write() calls work just like on an Arduino Uno; the one
// extra step is commit().

#include <EEPROM.h>

const int ADDRESS = 0;   // which byte of the emulated EEPROM to use

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  // Read the boot counter we saved last time. Erased/blank flash reads as 0xFF,
  // so treat that as the very first run.
  byte count = EEPROM.read(ADDRESS);
  if (count == 0xFF) {
    count = 0;
    Serial.println("first run");
  }

  count = count + 1;
  EEPROM.write(ADDRESS, count);   // stage the new value in RAM
  EEPROM.commit();                // flush to flash so it survives a reset

  Serial.print("boot count: ");
  Serial.println(count);          // goes up by one every time you reset
}

void loop() {
}
