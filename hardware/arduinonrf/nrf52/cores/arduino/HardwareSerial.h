#pragma once

#include "Stream.h"

class HardwareSerial : public Stream {
public:
    explicit HardwareSerial(bool usbBacked = false);

    void begin(unsigned long baudRate);
    void begin(unsigned long baudRate, uint16_t config);
    void end(void);
    int available(void) override;
    int read(void) override;
    int peek(void) override;
    void flush(void) override;
    size_t write(uint8_t value) override;
    int availableForWrite() override;
    using Print::write;
    explicit operator bool() const;

    bool isUsbBacked(void) const;
    bool connected(void) const;
    unsigned long baud(void) const;
    unsigned long baudRate(void) const;
    bool dtr() const;
    bool rts() const;

private:
    void configureUart();   // brings up UARTE0 (DMA) for the hardware Serial1
    unsigned long baudRate_;
    bool usbBacked_;
    bool enabled_;
};

extern HardwareSerial Serial;
extern HardwareSerial Serial1;
