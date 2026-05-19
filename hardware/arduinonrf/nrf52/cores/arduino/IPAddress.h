#pragma once

#include <stdint.h>

#include "Printable.h"

class Print;
class String;

class IPAddress : public Printable {
public:
    constexpr IPAddress()
        : octets_{0U, 0U, 0U, 0U} {
    }

    constexpr IPAddress(uint8_t first, uint8_t second, uint8_t third, uint8_t fourth)
        : octets_{first, second, third, fourth} {
    }

    explicit IPAddress(const uint8_t *address)
        : octets_{0U, 0U, 0U, 0U} {
        if (address != nullptr) {
            octets_[0] = address[0];
            octets_[1] = address[1];
            octets_[2] = address[2];
            octets_[3] = address[3];
        }
    }

    explicit IPAddress(uint32_t address)
        : octets_{
            static_cast<uint8_t>((address >> 24U) & 0xFFU),
            static_cast<uint8_t>((address >> 16U) & 0xFFU),
            static_cast<uint8_t>((address >> 8U) & 0xFFU),
            static_cast<uint8_t>(address & 0xFFU)} {
    }

    uint8_t operator[](int index) const {
        if (index >= 0 && index < 4) {
            return octets_[index];
        }
        return 0U;
    }

    uint8_t &operator[](int index) {
        static uint8_t dummy = 0U;
        if (index >= 0 && index < 4) {
            return octets_[index];
        }
        return dummy;
    }

    bool operator==(const IPAddress &other) const {
        return octets_[0] == other.octets_[0] && octets_[1] == other.octets_[1] && octets_[2] == other.octets_[2] && octets_[3] == other.octets_[3];
    }

    bool operator!=(const IPAddress &other) const {
        return !(*this == other);
    }

    bool operator==(uint32_t address) const {
        return static_cast<uint32_t>(*this) == address;
    }

    bool operator!=(uint32_t address) const {
        return !(*this == address);
    }

    IPAddress &operator=(uint32_t address) {
        octets_[0] = static_cast<uint8_t>((address >> 24U) & 0xFFU);
        octets_[1] = static_cast<uint8_t>((address >> 16U) & 0xFFU);
        octets_[2] = static_cast<uint8_t>((address >> 8U) & 0xFFU);
        octets_[3] = static_cast<uint8_t>(address & 0xFFU);
        return *this;
    }

    bool fromString(const char *text);
    bool fromString(const String &text);

    bool isSet() const {
        return octets_[0] != 0U || octets_[1] != 0U || octets_[2] != 0U || octets_[3] != 0U;
    }

    uint8_t *raw_address() {
        return octets_;
    }

    const uint8_t *raw_address() const {
        return octets_;
    }

    size_t printTo(Print &printer) const override;

    operator uint32_t() const {
        return (static_cast<uint32_t>(octets_[0]) << 24U) |
            (static_cast<uint32_t>(octets_[1]) << 16U) |
            (static_cast<uint32_t>(octets_[2]) << 8U) |
            static_cast<uint32_t>(octets_[3]);
    }

private:
    uint8_t octets_[4];
};