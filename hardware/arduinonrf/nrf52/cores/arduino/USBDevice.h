#pragma once

#include <stdbool.h>

#include "NrfUsbd.h"

class USBDeviceClass {
public:
    void init();
    void attach();
    void detach();
    void poll();
    bool initialized() const;
    bool attached() const;
    bool configured() const;
    bool connected() const;
    bool ready() const;
    bool suspended() const;
    bool wakeupHost();
    uint8_t address() const;
    uint8_t configuration() const;
    NrfUsbdStatus status() const;
    explicit operator bool() const;
};

extern USBDeviceClass USBDevice;