#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

class EEPROMClass {
public:
    static constexpr int kSize = 1024;

    class Cell {
    public:
        Cell(EEPROMClass *owner, int index) : owner_(owner), index_(index) {
        }

        Cell &operator=(uint8_t value) {
            if (owner_ != nullptr) {
                owner_->write(index_, value);
            }
            return *this;
        }

        Cell &operator=(const Cell &other) {
            return *this = static_cast<uint8_t>(other);
        }

        operator uint8_t() const {
            if (owner_ == nullptr) {
                return 0U;
            }
            return owner_->read(index_);
        }

        Cell &update(uint8_t value) {
            if (owner_ != nullptr) {
                owner_->update(index_, value);
            }
            return *this;
        }

    private:
        EEPROMClass *owner_;
        int index_;
    };

    bool begin(size_t length = kSize);
    void end();
    Cell operator[](int index);
    uint8_t operator[](int index) const;
    bool isValid(int index) const;
    size_t size() const;

    uint8_t read(int index) const;
    void write(int index, uint8_t value);
    void update(int index, uint8_t value);
    int length() const;
    bool commit();

    template <typename T, typename Validator>
    bool get(int index, T &value, const T &defaults, Validator validator) {
        get(index, value);
        if (validator(value)) {
            return true;
        }
        value = defaults;
        return false;
    }

    template <typename T>
    T &get(int index, T &value) {
        if (index < 0 || (index + static_cast<int>(sizeof(T))) > kSize) {
            return value;
        }
        memcpy(&value, storage_ + index, sizeof(T));
        return value;
    }

    template <typename T>
    const T &put(int index, const T &value) {
        if (index < 0 || (index + static_cast<int>(sizeof(T))) > kSize) {
            return value;
        }
        memcpy(storage_ + index, &value, sizeof(T));
        dirty_ = true;
        return value;
    }

private:
    static void ensureLoaded();
    static uintptr_t flashPageAddress();
    static uint32_t flashPageSize();
    static uint32_t checksum(const uint8_t *data, size_t length);
    static uint8_t storage_[kSize];
    static bool dirty_;
    static bool initialized_;
};

extern EEPROMClass EEPROM;
