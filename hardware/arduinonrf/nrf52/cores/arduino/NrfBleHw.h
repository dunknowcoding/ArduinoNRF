#pragma once

#include <stddef.h>
#include <stdint.h>

class NrfBleRadio {
public:
    void begin();
    void end();
    void setTxPower(int8_t powerDbm);
    void setAdvertisingIntervalUnits(uint16_t units);
    void setAddress(const uint8_t *address, size_t length);
    void setAdvertisingData(const uint8_t *data, size_t length);
    void startAdvertising();
    void stopAdvertising();
    bool isAdvertising() const;
    uint16_t advertisingIntervalUnits() const;

private:
    bool enabled_ = false;
    bool advertising_ = false;
    uint16_t intervalUnits_ = 160;
    uint8_t address_[6] = {0};
    uint8_t advData_[31] = {0};
    uint8_t advLength_ = 0;
};
