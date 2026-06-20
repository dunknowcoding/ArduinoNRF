#pragma once

#include <stddef.h>
#include <stdint.h>

class NrfUsbSerialBackend {
public:
    static constexpr size_t BufferSize = 256;

    void begin(unsigned long baudRate);
    void end();
    int available() const;
    int read();
    int peek() const;
    void flush();
    size_t write(uint8_t value);
    size_t write(const uint8_t *data, size_t length);  // block write (see NrfUsbd)
    bool connected() const;
    void setConnected(bool connected);
    void setLineCoding(unsigned long baudRate);
    void setLineState(bool dtr, bool rts);
    bool dtr() const;
    bool rts() const;
    unsigned long baud() const;
    size_t txPending() const;
    void injectRx(const uint8_t *data, size_t length);
    void poll();
    void configureFromSystemProfile();
    bool usbPeripheralEnabled() const;

private:
    void pushRx(uint8_t value);
    bool pushTx(uint8_t value);
    int popRx();

    unsigned long baudRate_ = 115200UL;
    bool connected_ = false;
    bool dtr_ = false;
    bool rts_ = false;
    uint8_t rxBuffer_[BufferSize] = {0};
    uint8_t txBuffer_[BufferSize] = {0};
    size_t rxHead_ = 0;
    size_t rxTail_ = 0;
    size_t txHead_ = 0;
    size_t txTail_ = 0;
};

NrfUsbSerialBackend &nrfUsbSerialBackend();