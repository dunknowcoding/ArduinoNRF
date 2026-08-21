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
    static unsigned long normalizedBaud(unsigned long baudRate);
};

NrfUsbSerialBackend &nrfUsbSerialBackend();
