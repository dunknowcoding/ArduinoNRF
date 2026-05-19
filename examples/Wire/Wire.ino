#include <Wire.h>

void setup() {
  const uint8_t payload[] = {0xAA, 0x55, 0x11};
  Wire.begin();
  Wire.setClock(400000);
  Wire.beginTransmission(0x42);
  Wire.write(payload, sizeof(payload));
  Wire.endTransmission();
}

void loop() {
  Wire.requestFrom(0x42, 4);
  while (Wire.available() > 0) {
    Wire.read();
  }
  delay(10);
}