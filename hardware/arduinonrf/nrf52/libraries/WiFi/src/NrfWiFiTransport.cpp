#include "NrfWiFiTransport.h"

#include <Arduino.h>
#include <SPI.h>

void NrfWiFiSpiTransport::begin(uint8_t chipSelectPin, uint8_t readyPin, uint8_t irqPin) {
    chipSelectPin_ = chipSelectPin;
    readyPin_ = readyPin;
    irqPin_ = irqPin;
    SPI.begin();
    pinMode(chipSelectPin_, OUTPUT);
    digitalWrite(chipSelectPin_, HIGH);
    if (readyPin_ != 0xFFU) {
        pinMode(readyPin_, INPUT);
    }
    if (irqPin_ != 0xFFU) {
        pinMode(irqPin_, INPUT);
    }
}

uint8_t NrfWiFiSpiTransport::transfer(uint8_t value) {
    select();
    SPI.beginTransaction(SPISettings(8000000UL, MSBFIRST, 0));
    const uint8_t result = SPI.transfer(value);
    SPI.endTransaction();
    deselect();
    return result;
}

void NrfWiFiSpiTransport::select() const {
    if (chipSelectPin_ != 0xFFU) {
        digitalWrite(chipSelectPin_, LOW);
    }
}

void NrfWiFiSpiTransport::deselect() const {
    if (chipSelectPin_ != 0xFFU) {
        digitalWrite(chipSelectPin_, HIGH);
    }
}

uint8_t NrfWiFiSpiTransport::chipSelectPin() const {
    return chipSelectPin_;
}

uint8_t NrfWiFiSpiTransport::readyPin() const {
    return readyPin_;
}

uint8_t NrfWiFiSpiTransport::irqPin() const {
    return irqPin_;
}

void Nrf7002WiFiDriver::begin(uint8_t chipSelectPin, uint8_t readyPin, uint8_t irqPin) {
    transport_.begin(chipSelectPin, readyPin, irqPin);
    configured_ = true;
}

bool Nrf7002WiFiDriver::configured() const {
    return configured_;
}

const NrfWiFiSpiTransport &Nrf7002WiFiDriver::transport() const {
    return transport_;
}
