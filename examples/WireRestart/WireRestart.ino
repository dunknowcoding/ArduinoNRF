#include <Wire.h>

void setup() {
  Wire.begin();
  Wire.setClock(400000);
}

void loop() {
  Wire.beginTransmission(0x42);
  Wire.write(0x10);
  Wire.write(0x20);
  Wire.endTransmission(false);

  Wire.requestFrom(0x42, 4, true);
  while (Wire.available() > 0) {
    Wire.read();
  }

  delay(10);
}