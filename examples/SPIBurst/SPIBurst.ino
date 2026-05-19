#include <SPI.h>

void setup() {
  SPI.begin();
}

void loop() {
  uint8_t buffer[] = {0x55, 0xAA, 0x11, 0x22};
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, 0));
  SPI.transfer16(0xA55A);
  SPI.transfer(buffer, sizeof(buffer));
  SPI.endTransaction();
  delay(10);
}