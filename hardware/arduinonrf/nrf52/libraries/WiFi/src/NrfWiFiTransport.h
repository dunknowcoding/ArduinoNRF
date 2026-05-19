#pragma once

#include <stdint.h>

class NrfWiFiSpiTransport {
public:
    void begin(uint8_t chipSelectPin, uint8_t readyPin = 0xFFU, uint8_t irqPin = 0xFFU);
    uint8_t transfer(uint8_t value);
    void select() const;
    void deselect() const;
    uint8_t chipSelectPin() const;
    uint8_t readyPin() const;
    uint8_t irqPin() const;

private:
    uint8_t chipSelectPin_ = 0xFFU;
    uint8_t readyPin_ = 0xFFU;
    uint8_t irqPin_ = 0xFFU;
};

class Nrf7002WiFiDriver {
public:
    void begin(uint8_t chipSelectPin, uint8_t readyPin = 0xFFU, uint8_t irqPin = 0xFFU);
    bool configured() const;
    const NrfWiFiSpiTransport &transport() const;

private:
    bool configured_ = false;
    NrfWiFiSpiTransport transport_;
};
