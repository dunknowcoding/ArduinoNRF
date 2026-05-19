// Applicable boards: nice!nano v2, SuperMini, and nRFMicro.
// This sketch uses the secondary SPI and Wire objects with the same standard calls as the primary buses.

#include <SPI.h>
#include <Wire.h>

void setup() {
#if defined(ARDUINO_NRF52_NICENANO_V2) || defined(ARDUINO_NRF52_SUPERMINI)
  Wire1.begin();
  Wire1.setClock(400000);
  Wire1.beginTransmission(0x08);
  Wire1.write(0x10);
  Wire1.write(0x20);
  Wire1.endTransmission();

  SPI1.begin();
  SPI1.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  SPI1.transfer(0x55);
  SPI1.transfer(0xAA);
  SPI1.endTransaction();
  SPI1.end();
  Wire1.end();
#elif defined(ARDUINO_NRF52_NRFMICRO)
  Wire1.begin();
  Wire1.setClock(400000);
  Wire1.beginTransmission(0x08);
  Wire1.write(0x10);
  Wire1.write(0x20);
  Wire1.endTransmission();

  SPI1.begin();
  SPI1.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  SPI1.transfer(0x55);
  SPI1.transfer(0xAA);
  SPI1.endTransaction();
  SPI1.end();
  Wire1.end();
#endif
}

void loop() {
  delay(50);
}