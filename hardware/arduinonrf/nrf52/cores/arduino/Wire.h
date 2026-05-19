#pragma once

#include <stddef.h>
#include <stdint.h>

#include "Stream.h"
#include <variant.h>

class TwoWire : public Stream {
public:
    using Print::write;

#ifndef WIRE_HAS_END
#define WIRE_HAS_END 1
#endif

    typedef void (*ReceiveCallback)(int);
    typedef void (*RequestCallback)(void);

    TwoWire();
    TwoWire(uint32_t peripheralBase, uint8_t sdaPin, uint8_t sclPin);

    void begin();
    void begin(uint8_t address);
    void end();
    void beginTransmission(uint8_t address);
    size_t write(uint8_t data) override;
    size_t write(const uint8_t *buffer, size_t length);
    uint8_t endTransmission(bool sendStop = true);
    uint8_t requestFrom(uint8_t address, uint8_t quantity, bool sendStop = true);
    uint8_t requestFrom(int address, int quantity, bool sendStop = true);
    int available(void) override;
    int read(void) override;
    int peek(void) override;
    void flush(void) override;
    void setClock(uint32_t frequency);
    void setWireTimeout(uint32_t timeout = 25000UL, bool resetWithTimeout = false);
    bool getWireTimeoutFlag(void) const;
    void clearWireTimeoutFlag(void);
    void onReceive(ReceiveCallback callback);
    void onRequest(RequestCallback callback);
    uint32_t configuredClockHz() const {
        return clockHz_;
    }
    uint8_t pinSDA() const {
        return sdaPin_;
    }
    uint8_t pinSCL() const {
        return sclPin_;
    }

private:
    bool ensureEnabled();
    void resetBus();
    bool stopTransaction();
    bool waitForEventOffset(uint32_t eventOffset);
    void handleTransferFailure();
    uint32_t timeoutSpins() const;
    uint8_t address_;
    uint8_t transactionAddress_ = 0;
    uint8_t buffer_[32];
    size_t length_;
    size_t readIndex_;
    uint32_t clockHz_;
    uint32_t timeoutMicros_;
    bool enabled_;
    bool timeoutFlag_;
    bool resetOnTimeout_;
    bool transactionOpen_ = false;
    uint32_t peripheralBase_;
    uint8_t sdaPin_;
    uint8_t sclPin_;
    ReceiveCallback receiveCallback_;
    RequestCallback requestCallback_;
};

extern TwoWire Wire;
extern TwoWire Wire1;
