#include <Arduino.h>
#include <SPI.h>

namespace {
constexpr uint8_t kChipSelectPin = SS;

void transferFrame(uint8_t *buffer, size_t length) {
  digitalWrite(kChipSelectPin, LOW);
  SPI.transfer(buffer, length);
  digitalWrite(kChipSelectPin, HIGH);
}
}

void setup() {
  pinMode(kChipSelectPin, OUTPUT);
  digitalWrite(kChipSelectPin, HIGH);
  SPI.begin();
}

void loop() {
  uint8_t readIdCommand[] = {0x9F, 0x00, 0x00, 0x00};
  uint8_t configCommand[] = {0x01, 0x12};

  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, 0));
  transferFrame(readIdCommand, sizeof(readIdCommand));

  digitalWrite(kChipSelectPin, LOW);
  SPI.transfer16(0xA55A);
  digitalWrite(kChipSelectPin, HIGH);

  transferFrame(configCommand, sizeof(configCommand));
  SPI.endTransaction();
  delay(10);
}