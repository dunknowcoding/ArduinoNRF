#include "Arduino.h"
#include "PeripheralLease.h"
#include "Wire.h"

namespace {
constexpr uint32_t TWI0_BASE = 0x40003000UL;
constexpr uint32_t TWI1_BASE = 0x40004000UL;
constexpr uint32_t NRF_PSEL_DISCONNECTED = 0xFFFFFFFFUL;
constexpr uint32_t TWI_TASKS_STARTRX = 0x000UL;
constexpr uint32_t TWI_TASKS_STARTTX = 0x008UL;
constexpr uint32_t TWI_TASKS_STOP = 0x014UL;
constexpr uint32_t TWI_EVENTS_STOPPED = 0x104UL;
constexpr uint32_t TWI_EVENTS_RXDREADY = 0x108UL;
constexpr uint32_t TWI_EVENTS_TXDSENT = 0x11CUL;
constexpr uint32_t TWI_EVENTS_ERROR = 0x124UL;
constexpr uint32_t TWI_ERRORSRC = 0x4C4UL;
constexpr uint32_t TWI_ENABLE = 0x500UL;
constexpr uint32_t TWI_PSELSCL = 0x508UL;
constexpr uint32_t TWI_PSELSDA = 0x50CUL;
constexpr uint32_t TWI_RXD = 0x518UL;
constexpr uint32_t TWI_TXD = 0x51CUL;
constexpr uint32_t TWI_FREQUENCY = 0x524UL;
constexpr uint32_t TWI_ADDRESS = 0x588UL;
constexpr uint32_t TWI_ENABLE_DISABLED = 0UL;
constexpr uint32_t TWI_ENABLE_ENABLED = 5UL;
constexpr uint32_t TWI_FREQUENCY_100K = 0x01980000UL;
constexpr uint32_t TWI_FREQUENCY_250K = 0x04000000UL;
constexpr uint32_t TWI_FREQUENCY_400K = 0x06400000UL;
constexpr uint32_t TWI_TIMEOUT_SPINS = 200000UL;

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
    if (clockHz >= 400000UL) {
        return TWI_FREQUENCY_400K;
    }
    if (clockHz >= 250000UL) {
        return TWI_FREQUENCY_250K;
    }
    return TWI_FREQUENCY_100K;
}

bool boardPinUsable(uint8_t boardPin) {
    return boardPin != static_cast<uint8_t>(0xFFU) && boardPin < PINS_COUNT;
}

bool waitForEvent(uint32_t base, uint32_t eventOffset, uint32_t timeoutSpins) {
    uint32_t spinLimit = timeoutSpins;
    if (spinLimit == 0UL) {
        spinLimit = TWI_TIMEOUT_SPINS;
    }
    for (uint32_t spin = 0; spin < spinLimit; ++spin) {
        if (reg32(base, TWI_EVENTS_ERROR) != 0UL) {
            return false;
        }
        if (reg32(base, eventOffset) != 0UL) {
            return true;
        }
    }
    return false;
}

void clearEvent(uint32_t base, uint32_t eventOffset) {
    reg32(base, eventOffset) = 0UL;
}

void recoverBusLines(uint8_t sdaPin, uint8_t sclPin) {
    if (!boardPinUsable(sdaPin) || !boardPinUsable(sclPin)) {
        return;
    }

    pinMode(sdaPin, INPUT_PULLUP);
    pinMode(sclPin, INPUT_PULLUP);
    if (digitalRead(sdaPin) == HIGH && digitalRead(sclPin) == HIGH) {
        return;
    }

    pinMode(sclPin, OUTPUT);
    digitalWrite(sclPin, HIGH);
    for (uint8_t pulse = 0; pulse < 9U && digitalRead(sdaPin) == LOW; ++pulse) {
        digitalWrite(sclPin, LOW);
        delayMicroseconds(4U);
        digitalWrite(sclPin, HIGH);
        delayMicroseconds(4U);
    }

    pinMode(sdaPin, OUTPUT);
    digitalWrite(sdaPin, LOW);
    delayMicroseconds(4U);
    digitalWrite(sclPin, HIGH);
    delayMicroseconds(4U);
    digitalWrite(sdaPin, HIGH);
    delayMicroseconds(4U);
    pinMode(sdaPin, INPUT_PULLUP);
    pinMode(sclPin, INPUT_PULLUP);
}
}

TwoWire Wire(TWI0_BASE, PIN_WIRE_SDA, PIN_WIRE_SCL);
TwoWire Wire1(TWI1_BASE, PIN_WIRE1_SDA, PIN_WIRE1_SCL);

TwoWire::TwoWire()
    : TwoWire(TWI0_BASE, PIN_WIRE_SDA, PIN_WIRE_SCL) {
}

TwoWire::TwoWire(uint32_t peripheralBase, uint8_t sdaPin, uint8_t sclPin)
    : address_(0), buffer_{0}, length_(0), readIndex_(0), clockHz_(100000UL), timeoutMicros_(25000UL), enabled_(false),
      timeoutFlag_(false), resetOnTimeout_(false), peripheralBase_(peripheralBase), sdaPin_(sdaPin), sclPin_(sclPin),
      receiveCallback_(nullptr), requestCallback_(nullptr) {
}

void TwoWire::begin() {
    length_ = 0;
    readIndex_ = 0;
    timeoutFlag_ = false;
    ensureEnabled();
}

void TwoWire::begin(uint8_t address) {
    address_ = address;
    begin();
}

void TwoWire::end() {
    length_ = 0;
    readIndex_ = 0;
    if (nrfPeripheralOwnerIs(peripheralBase_, this)) {
        reg32(peripheralBase_, TWI_ENABLE) = TWI_ENABLE_DISABLED;
        reg32(peripheralBase_, TWI_PSELSCL) = NRF_PSEL_DISCONNECTED;
        reg32(peripheralBase_, TWI_PSELSDA) = NRF_PSEL_DISCONNECTED;
        nrfPeripheralRelease(peripheralBase_, this);
    }
    enabled_ = false;
    transactionOpen_ = false;
    timeoutFlag_ = false;
}

void TwoWire::beginTransmission(uint8_t address) {
    if (transactionOpen_ && transactionAddress_ != address) {
        stopTransaction();
    }
    address_ = address;
    length_ = 0;
    readIndex_ = 0;
}

size_t TwoWire::write(uint8_t data) {
    if (length_ >= sizeof(buffer_)) {
        return 0;
    }

    buffer_[length_] = data;
    ++length_;
    return 1;
}

size_t TwoWire::write(const uint8_t *buffer, size_t length) {
    size_t written = 0;
    while (written < length) {
        written += write(buffer[written]);
        if (written < length && length_ >= sizeof(buffer_)) {
            break;
        }
    }
    return written;
}

uint8_t TwoWire::endTransmission(bool sendStop) {
    if (!ensureEnabled()) {
        return 4;
    }

    reg32(peripheralBase_, TWI_ADDRESS) = address_;
    reg32(peripheralBase_, TWI_ERRORSRC) = 0xFFFFFFFFUL;
    clearEvent(peripheralBase_, TWI_EVENTS_ERROR);
    clearEvent(peripheralBase_, TWI_EVENTS_TXDSENT);
    clearEvent(peripheralBase_, TWI_EVENTS_STOPPED);
    reg32(peripheralBase_, TWI_TASKS_STARTTX) = 1UL;

    for (size_t index = 0; index < length_; ++index) {
        reg32(peripheralBase_, TWI_TXD) = buffer_[index];
        clearEvent(peripheralBase_, TWI_EVENTS_TXDSENT);
        if (!waitForEventOffset(TWI_EVENTS_TXDSENT)) {
            if (sendStop) {
                reg32(peripheralBase_, TWI_TASKS_STOP) = 1UL;
                (void)waitForEventOffset(TWI_EVENTS_STOPPED);
            }
            return 4;
        }
    }

    if (sendStop) {
        if (!stopTransaction()) {
            resetBus();
            return 4;
        }
        transactionOpen_ = false;
    } else {
        transactionOpen_ = true;
        transactionAddress_ = address_;
    }

    return 0;
}

uint8_t TwoWire::requestFrom(uint8_t address, uint8_t quantity, bool sendStop) {
    if (!ensureEnabled()) {
        return 0;
    }

    if (transactionOpen_ && transactionAddress_ != address) {
        if (!stopTransaction()) {
            resetBus();
            return 0;
        }
    }

    length_ = quantity;
    if (length_ > sizeof(buffer_)) {
        length_ = sizeof(buffer_);
    }
    readIndex_ = 0;

    reg32(peripheralBase_, TWI_ADDRESS) = address;
    reg32(peripheralBase_, TWI_ERRORSRC) = 0xFFFFFFFFUL;
    clearEvent(peripheralBase_, TWI_EVENTS_ERROR);
    clearEvent(peripheralBase_, TWI_EVENTS_RXDREADY);
    clearEvent(peripheralBase_, TWI_EVENTS_STOPPED);
    reg32(peripheralBase_, TWI_TASKS_STARTRX) = 1UL;

    for (size_t index = 0; index < length_; ++index) {
        clearEvent(peripheralBase_, TWI_EVENTS_RXDREADY);
        if (!waitForEventOffset(TWI_EVENTS_RXDREADY)) {
            length_ = index;
            break;
        }
        buffer_[index] = static_cast<uint8_t>(reg32(peripheralBase_, TWI_RXD) & 0xFFUL);
    }

    if (sendStop) {
        if (!stopTransaction()) {
            resetBus();
            return static_cast<uint8_t>(length_);
        }
        transactionOpen_ = false;
    } else {
        transactionOpen_ = true;
        transactionAddress_ = address;
    }

    return static_cast<uint8_t>(length_);
}

int TwoWire::available(void) {
    return static_cast<int>(length_ - readIndex_);
}

int TwoWire::read(void) {
    if (readIndex_ >= length_) {
        return -1;
    }

    int value = buffer_[readIndex_];
    ++readIndex_;
    return value;
}

int TwoWire::peek(void) {
    if (readIndex_ >= length_) {
        return -1;
    }

    return buffer_[readIndex_];
}

void TwoWire::flush(void) {
}

void TwoWire::setClock(uint32_t frequency) {
    clockHz_ = frequency;
    if (enabled_ && nrfPeripheralOwnerIs(peripheralBase_, this)) {
        reg32(peripheralBase_, TWI_FREQUENCY) = frequencyRegisterValue(clockHz_);
    }
}

void TwoWire::setWireTimeout(uint32_t timeout, bool resetWithTimeout) {
    timeoutMicros_ = timeout;
    resetOnTimeout_ = resetWithTimeout;
    timeoutFlag_ = false;
}

bool TwoWire::getWireTimeoutFlag(void) const {
    return timeoutFlag_;
}

void TwoWire::clearWireTimeoutFlag(void) {
    timeoutFlag_ = false;
}

void TwoWire::onReceive(ReceiveCallback callback) {
    receiveCallback_ = callback;
}

void TwoWire::onRequest(RequestCallback callback) {
    requestCallback_ = callback;
}

uint8_t TwoWire::requestFrom(int address, int quantity, bool sendStop) {
    if (address < 0 || quantity <= 0) {
        return 0;
    }

    return requestFrom(static_cast<uint8_t>(address), static_cast<uint8_t>(quantity), sendStop);
}

bool TwoWire::ensureEnabled() {
    if (enabled_ && nrfPeripheralOwnerIs(peripheralBase_, this)) {
        return true;
    }

    const uint32_t rawSda = rawPinFromBoardPin(sdaPin_);
    const uint32_t rawScl = rawPinFromBoardPin(sclPin_);
    if (rawSda == NRF_PSEL_DISCONNECTED || rawScl == NRF_PSEL_DISCONNECTED) {
        return false;
    }

    recoverBusLines(sdaPin_, sclPin_);
    pinMode(sdaPin_, INPUT_PULLUP);
    pinMode(sclPin_, INPUT_PULLUP);
    nrfPeripheralTake(peripheralBase_, this);
    reg32(peripheralBase_, TWI_ENABLE) = TWI_ENABLE_DISABLED;
    reg32(peripheralBase_, TWI_PSELSDA) = rawSda;
    reg32(peripheralBase_, TWI_PSELSCL) = rawScl;
    reg32(peripheralBase_, TWI_FREQUENCY) = frequencyRegisterValue(clockHz_);
    reg32(peripheralBase_, TWI_ENABLE) = TWI_ENABLE_ENABLED;
    enabled_ = true;
    return true;
}

void TwoWire::resetBus() {
    if (nrfPeripheralOwnerIs(peripheralBase_, this)) {
        reg32(peripheralBase_, TWI_TASKS_STOP) = 1UL;
        reg32(peripheralBase_, TWI_ENABLE) = TWI_ENABLE_DISABLED;
        nrfPeripheralRelease(peripheralBase_, this);
    }
    enabled_ = false;
    transactionOpen_ = false;
    transactionAddress_ = 0;
    recoverBusLines(sdaPin_, sclPin_);
    (void)ensureEnabled();
}

bool TwoWire::stopTransaction() {
    reg32(peripheralBase_, TWI_TASKS_STOP) = 1UL;
    clearEvent(peripheralBase_, TWI_EVENTS_STOPPED);
    if (waitForEventOffset(TWI_EVENTS_STOPPED)) {
        transactionOpen_ = false;
        transactionAddress_ = 0;
        return true;
    }
    return false;
}

bool TwoWire::waitForEventOffset(uint32_t eventOffset) {
    if (waitForEvent(peripheralBase_, eventOffset, timeoutSpins())) {
        return true;
    }

    handleTransferFailure();
    return false;
}

void TwoWire::handleTransferFailure() {
    timeoutFlag_ = true;
    if (resetOnTimeout_) {
        resetBus();
    }
}

uint32_t TwoWire::timeoutSpins() const {
    if (timeoutMicros_ == 0UL) {
        return TWI_TIMEOUT_SPINS;
    }

    const uint32_t scaled = timeoutMicros_ * 8UL;
    if (scaled < 1024UL) {
        return 1024UL;
    }
    return scaled;
}
