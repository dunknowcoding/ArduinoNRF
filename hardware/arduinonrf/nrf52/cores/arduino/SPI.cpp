#include "Arduino.h"
#include "PeripheralLease.h"
#include "SPI.h"

namespace {
constexpr uint32_t SPI1_BASE = 0x40004000UL;
constexpr uint32_t SPI2_BASE = 0x40023000UL;
constexpr uint32_t NRF_PSEL_DISCONNECTED = 0xFFFFFFFFUL;
constexpr uint32_t SPI_EVENTS_READY = 0x108UL;
constexpr uint32_t SPI_ENABLE = 0x500UL;
constexpr uint32_t SPI_PSELSCK = 0x508UL;
constexpr uint32_t SPI_PSELMOSI = 0x50CUL;
constexpr uint32_t SPI_PSELMISO = 0x510UL;
constexpr uint32_t SPI_RXD = 0x518UL;
constexpr uint32_t SPI_TXD = 0x51CUL;
constexpr uint32_t SPI_FREQUENCY = 0x524UL;
constexpr uint32_t SPI_CONFIG = 0x554UL;
constexpr uint32_t SPI_ENABLE_DISABLED = 0UL;
constexpr uint32_t SPI_ENABLE_ENABLED = 1UL;
constexpr uint32_t SPI_TIMEOUT_SPINS = 200000UL;
constexpr uint32_t SPI_MAX_CLOCK_HZ = 8000000UL;

inline volatile uint32_t &reg32(uint32_t base, uint32_t offset) {
    return *reinterpret_cast<volatile uint32_t *>(base + offset);
}

inline uint32_t rawPinFromBoardPin(uint8_t boardPin) {
    if (boardPin < PINS_COUNT) {
        return g_ADigitalPinMap[boardPin];
    }
    return NRF_PSEL_DISCONNECTED;
}

inline uint32_t frequencyRegisterValue(uint32_t clockHz) {
    if (clockHz >= 8000000UL) {
        return 0x80000000UL;
    }
    if (clockHz >= 4000000UL) {
        return 0x40000000UL;
    }
    if (clockHz >= 2000000UL) {
        return 0x20000000UL;
    }
    if (clockHz >= 1000000UL) {
        return 0x10000000UL;
    }
    if (clockHz >= 500000UL) {
        return 0x08000000UL;
    }
    if (clockHz >= 250000UL) {
        return 0x04000000UL;
    }
    return 0x02000000UL;
}

inline uint32_t configRegisterValue(const SPISettings &settings) {
    uint32_t lsbFirst = 0UL;
    if (settings.bitOrderValue == LSBFIRST) {
        lsbFirst = 1UL;
    }
    uint32_t cpha = 0UL;
    if ((settings.dataModeValue & 0x01U) != 0U) {
        cpha = (1UL << 1);
    }
    uint32_t cpol = 0UL;
    if ((settings.dataModeValue & 0x02U) != 0U) {
        cpol = (1UL << 2);
    }
    return lsbFirst | cpha | cpol;
}

bool waitReady(uint32_t peripheralBase) {
    for (uint32_t spin = 0; spin < SPI_TIMEOUT_SPINS; ++spin) {
        if (reg32(peripheralBase, SPI_EVENTS_READY) != 0UL) {
            return true;
        }
    }
    return false;
}
}

SPIClass SPI(SPI2_BASE, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SPI_SCK);
SPIClass SPI1(SPI1_BASE, PIN_SPI1_MISO, PIN_SPI1_MOSI, PIN_SPI1_SCK);

SPIClass::SPIClass() : SPIClass(SPI2_BASE, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SPI_SCK) {
}

SPIClass::SPIClass(uint32_t peripheralBase, uint8_t misoPin, uint8_t mosiPin, uint8_t sckPin)
    : currentSettings_(), enabled_(false), peripheralBase_(peripheralBase), misoPin_(misoPin), mosiPin_(mosiPin), sckPin_(sckPin) {
}

void SPIClass::begin() {
    if (!ensureEnabled()) {
        return;
    }
}

void SPIClass::begin(uint8_t sckPin, uint8_t misoPin, uint8_t mosiPin, uint8_t ssPin) {
    if (enabled_ && (sckPin_ != sckPin || misoPin_ != misoPin || mosiPin_ != mosiPin)) {
        end();
    }
    sckPin_ = sckPin;
    misoPin_ = misoPin;
    mosiPin_ = mosiPin;
    begin();
    // Convenience only: park the chip-select idle-high. SPI never drives it
    // during transfers - the sketch toggles it around each transaction.
    if (ssPin != 0xFFU) {
        pinMode(ssPin, OUTPUT);
        digitalWrite(ssPin, HIGH);
    }
}

bool SPIClass::ensureEnabled() {
    if (enabled_ && nrfPeripheralOwnerIs(peripheralBase_, this)) {
        return true;
    }

    const uint32_t rawSck = rawPinFromBoardPin(sckPin_);
    const uint32_t rawMosi = rawPinFromBoardPin(mosiPin_);
    const uint32_t rawMiso = rawPinFromBoardPin(misoPin_);
    if (rawSck == NRF_PSEL_DISCONNECTED || rawMosi == NRF_PSEL_DISCONNECTED || rawMiso == NRF_PSEL_DISCONNECTED) {
        enabled_ = false;
        return false;
    }

    nrfPeripheralTake(peripheralBase_, this);
    reg32(peripheralBase_, SPI_ENABLE) = SPI_ENABLE_DISABLED;
    reg32(peripheralBase_, SPI_PSELSCK) = rawSck;
    reg32(peripheralBase_, SPI_PSELMOSI) = rawMosi;
    reg32(peripheralBase_, SPI_PSELMISO) = rawMiso;
    pinMode(sckPin_, OUTPUT);
    pinMode(mosiPin_, OUTPUT);
    pinMode(misoPin_, INPUT);
    applySettings();
    reg32(peripheralBase_, SPI_ENABLE) = SPI_ENABLE_ENABLED;
    enabled_ = true;
    return true;
}

void SPIClass::end() {
    if (nrfPeripheralOwnerIs(peripheralBase_, this)) {
        reg32(peripheralBase_, SPI_ENABLE) = SPI_ENABLE_DISABLED;
        reg32(peripheralBase_, SPI_PSELSCK) = NRF_PSEL_DISCONNECTED;
        reg32(peripheralBase_, SPI_PSELMOSI) = NRF_PSEL_DISCONNECTED;
        reg32(peripheralBase_, SPI_PSELMISO) = NRF_PSEL_DISCONNECTED;
        nrfPeripheralRelease(peripheralBase_, this);
    }
    enabled_ = false;
    transactionActive_ = false;
}

void SPIClass::beginTransaction(const SPISettings &settings) {
    currentSettings_ = settings;
    transactionActive_ = true;
    if (!ensureEnabled()) {
        return;
    }
    applySettings();
}

void SPIClass::endTransaction() {
    transactionActive_ = false;
}

uint8_t SPIClass::transfer(uint8_t data) {
    if (!ensureEnabled()) {
        return 0;
    }

    reg32(peripheralBase_, SPI_EVENTS_READY) = 0UL;
    reg32(peripheralBase_, SPI_TXD) = data;
    if (!waitReady(peripheralBase_)) {
        recoverBus();
        return 0;
    }
    return static_cast<uint8_t>(reg32(peripheralBase_, SPI_RXD) & 0xFFUL);
}

uint16_t SPIClass::transfer16(uint16_t data) {
    uint16_t value = 0U;
    if (currentSettings_.bitOrderValue == LSBFIRST) {
        value = static_cast<uint16_t>(transfer(static_cast<uint8_t>(data & 0xFFU)));
        value |= static_cast<uint16_t>(transfer(static_cast<uint8_t>((data >> 8U) & 0xFFU))) << 8U;
    } else {
        value = static_cast<uint16_t>(transfer(static_cast<uint8_t>((data >> 8U) & 0xFFU))) << 8U;
        value |= static_cast<uint16_t>(transfer(static_cast<uint8_t>(data & 0xFFU)));
    }
    return value;
}

void SPIClass::transfer(void *buffer, size_t size) {
    if (buffer == nullptr) {
        return;
    }

    uint8_t *bytes = reinterpret_cast<uint8_t *>(buffer);
    for (size_t index = 0; index < size; ++index) {
        bytes[index] = transfer(bytes[index]);
    }
}

void SPIClass::transfer(const void *txBuffer, void *rxBuffer, size_t size) {
    const uint8_t *txBytes = reinterpret_cast<const uint8_t *>(txBuffer);
    uint8_t *rxBytes = reinterpret_cast<uint8_t *>(rxBuffer);

    for (size_t index = 0; index < size; ++index) {
        uint8_t value = 0xFFU;
        if (txBytes != nullptr) {
            value = txBytes[index];
        }
        const uint8_t received = transfer(value);
        if (rxBytes != nullptr) {
            rxBytes[index] = received;
        }
    }
}

void SPIClass::setBitOrder(uint8_t bitOrder) {
    currentSettings_.bitOrderValue = bitOrder;
    if (enabled_ && nrfPeripheralOwnerIs(peripheralBase_, this)) {
        applySettings();
    }
}

void SPIClass::setDataMode(uint8_t dataMode) {
    currentSettings_.dataModeValue = dataMode;
    if (enabled_ && nrfPeripheralOwnerIs(peripheralBase_, this)) {
        applySettings();
    }
}

void SPIClass::setClockDivider(uint8_t divider) {
    if (divider == 0U) {
        currentSettings_.clockHz = 4000000UL;
    } else {
        uint32_t derivedClock = 16000000UL / static_cast<uint32_t>(divider);
        currentSettings_.clockHz = derivedClock;
        if (currentSettings_.clockHz > SPI_MAX_CLOCK_HZ) {
            currentSettings_.clockHz = SPI_MAX_CLOCK_HZ;
        }
    }
    if (enabled_ && nrfPeripheralOwnerIs(peripheralBase_, this)) {
        applySettings();
    }
}

void SPIClass::usingInterrupt(uint8_t interruptNumber) {
    (void)interruptNumber;
}

void SPIClass::notUsingInterrupt(uint8_t interruptNumber) {
    (void)interruptNumber;
}

void SPIClass::attachInterrupt() {
}

void SPIClass::detachInterrupt() {
}

bool SPIClass::isEnabled() const {
    return enabled_;
}

uint32_t SPIClass::configuredClockHz() const {
    return currentSettings_.clockHz;
}

uint32_t SPIClass::maxClockHz() const {
    return SPI_MAX_CLOCK_HZ;
}

bool SPIClass::supportsClockHz(uint32_t clockHz) const {
    return clockHz > 0UL && clockHz <= SPI_MAX_CLOCK_HZ;
}

void SPIClass::recoverBus() {
    if (nrfPeripheralOwnerIs(peripheralBase_, this)) {
        reg32(peripheralBase_, SPI_ENABLE) = SPI_ENABLE_DISABLED;
        nrfPeripheralRelease(peripheralBase_, this);
    }
    enabled_ = false;
    if (transactionActive_) {
        (void)ensureEnabled();
        applySettings();
    }
}

void SPIClass::applySettings() {
    reg32(peripheralBase_, SPI_FREQUENCY) = frequencyRegisterValue(currentSettings_.clockHz);
    reg32(peripheralBase_, SPI_CONFIG) = configRegisterValue(currentSettings_);
}
