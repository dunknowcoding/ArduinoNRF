#include "NrfBleHw.h"

#include <string.h>

#include "NrfClock.h"

namespace {
constexpr uint32_t RADIO_BASE = 0x40001000UL;
constexpr uint32_t RADIO_TXPOWER = 0x50CUL;
constexpr uint32_t RADIO_MODE = 0x510UL;
constexpr uint32_t RADIO_PREFIX0 = 0x524UL;
constexpr uint32_t RADIO_BASE0 = 0x51CUL;
constexpr uint32_t RADIO_PCNF0 = 0x514UL;
constexpr uint32_t RADIO_PCNF1 = 0x518UL;
constexpr uint32_t RADIO_MODECNF0 = 0x650UL;
constexpr uint32_t RADIO_POWER = 0xFFCUL;
constexpr uint32_t RADIO_POWER_ENABLED = 1UL;
constexpr uint32_t RADIO_POWER_DISABLED = 0UL;
constexpr uint32_t RADIO_MODE_BLE_1MBIT = 3UL;

inline volatile uint32_t &reg32(uint32_t base, uint32_t offset) {
    return *reinterpret_cast<volatile uint32_t *>(base + offset);
}

inline uint32_t txPowerRegisterValue(int8_t powerDbm) {
    if (powerDbm >= 8) {
        return 0x08UL;
    }
    if (powerDbm >= 4) {
        return 0x04UL;
    }
    if (powerDbm >= 0) {
        return 0x00UL;
    }
    if (powerDbm >= -4) {
        return 0xFCUL;
    }
    if (powerDbm >= -8) {
        return 0xF8UL;
    }
    if (powerDbm >= -12) {
        return 0xF4UL;
    }
    if (powerDbm >= -16) {
        return 0xF0UL;
    }
    return 0xECUL;
}
}

void NrfBleRadio::begin() {
    nrfStartLfclk();
    nrfStartHfclk();
    reg32(RADIO_BASE, RADIO_POWER) = RADIO_POWER_ENABLED;
    reg32(RADIO_BASE, RADIO_MODE) = RADIO_MODE_BLE_1MBIT;
    reg32(RADIO_BASE, RADIO_MODECNF0) = 0UL;
    reg32(RADIO_BASE, RADIO_PCNF0) = 0x00000008UL;
    reg32(RADIO_BASE, RADIO_PCNF1) = 0x02030025UL;
    enabled_ = true;
}

void NrfBleRadio::end() {
    advertising_ = false;
    enabled_ = false;
    reg32(RADIO_BASE, RADIO_POWER) = RADIO_POWER_DISABLED;
}

void NrfBleRadio::setTxPower(int8_t powerDbm) {
    if (!enabled_) {
        begin();
    }
    reg32(RADIO_BASE, RADIO_TXPOWER) = txPowerRegisterValue(powerDbm);
}

void NrfBleRadio::setAdvertisingIntervalUnits(uint16_t units) {
    intervalUnits_ = units;
    if (intervalUnits_ < 32U) {
        intervalUnits_ = 32U;
    }
}

void NrfBleRadio::setAddress(const uint8_t *address, size_t length) {
    if (address == nullptr || length < 6) {
        return;
    }
    memcpy(address_, address, 6);
    reg32(RADIO_BASE, RADIO_PREFIX0) = address_[5];
    reg32(RADIO_BASE, RADIO_BASE0) = (static_cast<uint32_t>(address_[0]) << 24) |
                                     (static_cast<uint32_t>(address_[1]) << 16) |
                                     (static_cast<uint32_t>(address_[2]) << 8) |
                                     static_cast<uint32_t>(address_[3]);
}

void NrfBleRadio::setAdvertisingData(const uint8_t *data, size_t length) {
    if (data == nullptr) {
        advLength_ = 0;
        return;
    }
    if (length > sizeof(advData_)) {
        length = sizeof(advData_);
    }
    memcpy(advData_, data, length);
    advLength_ = static_cast<uint8_t>(length);
}

void NrfBleRadio::startAdvertising() {
    if (!enabled_) {
        begin();
    }
    advertising_ = true;
}

void NrfBleRadio::stopAdvertising() {
    advertising_ = false;
}

bool NrfBleRadio::isAdvertising() const {
    return advertising_;
}

uint16_t NrfBleRadio::advertisingIntervalUnits() const {
    return intervalUnits_;
}
