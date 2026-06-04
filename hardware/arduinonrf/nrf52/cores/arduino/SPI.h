#pragma once

#include <stddef.h>
#include <stdint.h>

#include <variant.h>

#ifndef NOT_A_PIN
#define NOT_A_PIN 0xFFu
#endif

#ifndef SPI_HAS_TRANSACTION
#define SPI_HAS_TRANSACTION 1
#endif

#ifndef SPI_HAS_NOTUSINGINTERRUPT
#define SPI_HAS_NOTUSINGINTERRUPT 1
#endif

#ifndef SPI_MODE0
#define SPI_MODE0 0x00u
#define SPI_MODE1 0x01u
#define SPI_MODE2 0x02u
#define SPI_MODE3 0x03u
#endif

#ifndef SPI_CLOCK_DIV2
#define SPI_CLOCK_DIV2 2u
#define SPI_CLOCK_DIV4 4u
#define SPI_CLOCK_DIV8 8u
#define SPI_CLOCK_DIV16 16u
#define SPI_CLOCK_DIV32 32u
#define SPI_CLOCK_DIV64 64u
#define SPI_CLOCK_DIV128 128u
#endif

class SPISettings {
public:
    SPISettings(uint32_t clock = 4000000UL, uint8_t bitOrder = 1, uint8_t dataMode = 0)
        : clockHz(clock), bitOrderValue(bitOrder), dataModeValue(dataMode) {
    }

    uint32_t clockHz;
    uint8_t bitOrderValue;
    uint8_t dataModeValue;
};

class SPIClass {
public:
    SPIClass();
    SPIClass(uint32_t peripheralBase, uint8_t misoPin, uint8_t mosiPin, uint8_t sckPin);

    void begin();
    // Begin on explicit pins (Arduino "Dn" numbers), in (SCK, MISO, MOSI[, SS])
    // order, e.g. SPI.begin(SCK, MISO, MOSI) or SPI.begin(2, 3, 4, 5).
    // NOTE on chip-select: SPI (like every Arduino SPI core) does NOT drive CS
    // automatically - you toggle it yourself around a transfer:
    //     digitalWrite(cs, LOW); SPI.transfer(...); digitalWrite(cs, HIGH);
    // Passing the optional ssPin here is just a convenience: it is configured as
    // an OUTPUT driven HIGH (idle). Leave it off if you manage CS yourself.
    void begin(uint8_t sckPin, uint8_t misoPin, uint8_t mosiPin, uint8_t ssPin = 0xFFU);
    void end();
    void beginTransaction(const SPISettings &settings);
    void endTransaction();
    uint8_t transfer(uint8_t data);
    uint16_t transfer16(uint16_t data);
    void transfer(void *buffer, size_t size);
    void transfer(const void *txBuffer, void *rxBuffer, size_t size);
    void setBitOrder(uint8_t bitOrder);
    void setDataMode(uint8_t dataMode);
    void setClockDivider(uint8_t divider);
    void usingInterrupt(uint8_t interruptNumber);
    void notUsingInterrupt(uint8_t interruptNumber);
    void attachInterrupt();
    void detachInterrupt();
    bool isEnabled() const;
    uint32_t configuredClockHz() const;
    uint32_t maxClockHz() const;
    bool supportsClockHz(uint32_t clockHz) const;
    uint8_t pinMISO() const {
        return misoPin_;
    }
    uint8_t pinMOSI() const {
        return mosiPin_;
    }
    uint8_t pinSCK() const {
        return sckPin_;
    }
    uint8_t pinSS() const {
        return NOT_A_PIN;
    }

private:
    bool ensureEnabled();
    void recoverBus();
    void applySettings();
    SPISettings currentSettings_;
    bool enabled_;
    uint32_t peripheralBase_;
    uint8_t misoPin_;
    uint8_t mosiPin_;
    uint8_t sckPin_;
    bool transactionActive_ = false;
};

extern SPIClass SPI;
extern SPIClass SPI1;
