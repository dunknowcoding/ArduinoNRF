#pragma once

#include "Stream.h"

// Always-present service/debug CDC serial port backed by the first (fixed)
// CDC interface pair in the dual-CDC USB descriptor.  This port is active
// whenever the USB peripheral is running, regardless of whether the user CDC
// port is enabled via the usbcdc boards.txt menu.  Use it for diagnostic
// output that must be visible even when usbcdc=disabled, and as the anchor
// port for 1200 bps DFU touch in applications that disable the user CDC.
class NrfServiceSerial : public Stream {
public:
    // begin() ensures the USB peripheral is started.  Calling it is optional;
    // the port is also started automatically by the board init() call via
    // initVariant().  The baud parameter is accepted for API compatibility but
    // has no effect — the service CDC baud rate is negotiated by the host.
    void begin(unsigned long baud = 115200);
    void end();

    int available() override;
    int read() override;
    int peek() override;
    void flush() override;
    size_t write(uint8_t value) override;
    int availableForWrite() override;
    using Print::write;

    // Returns true when the USB device is in the Configured state (the host
    // has completed enumeration and sent SET_CONFIGURATION).  Suitable for use
    // as a boolean guard: `while (!SerialService) { ... }`.
    explicit operator bool() const;

    // connected() reflects the DTR line set by the host terminal on the
    // service CDC port.  Use this to detect whether a terminal is open.
    bool connected() const;

    bool dtr() const;
    bool rts() const;
    unsigned long baud() const;
};

extern NrfServiceSerial SerialService;
