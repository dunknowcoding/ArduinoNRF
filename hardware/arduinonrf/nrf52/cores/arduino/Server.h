#pragma once

#include <stdint.h>

#include "Print.h"

class Server : public Print {
public:
    using Print::write;

    virtual ~Server() = default;
    virtual void begin() = 0;
    virtual void begin(uint16_t port) {
        (void)port;
        begin();
    }
};