// Applicable boards: all packaged boards.
// This sketch shows standard SPI and Wire configuration calls without relying on board-specific inspection helpers.

#include <SPI.h>
#include <Wire.h>

void setup() {
  SPI.begin();
  SPI.beginTransaction(SPISettings(250000, MSBFIRST, SPI_MODE0));
  SPI.transfer(0x55);
  SPI.endTransaction();

  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE3));
  SPI.transfer(0xAA);
  SPI.endTransaction();

  Wire.begin();
  Wire.setClock(100000);
  Wire.beginTransmission(0x08);
  Wire.write(0x55);
  Wire.endTransmission();

  Wire.setClock(400000);
  Wire.beginTransmission(0x08);
  Wire.write(0xAA);
  Wire.endTransmission();
}

void loop() {
  delay(50);
}