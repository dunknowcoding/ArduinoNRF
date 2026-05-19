#include <SPI.h>

void setup() {
  SPI.begin();
}

void loop() {
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  SPI.transfer(0x55);
  SPI.transfer(0xAA);
  SPI.endTransaction();
  delay(10);
}