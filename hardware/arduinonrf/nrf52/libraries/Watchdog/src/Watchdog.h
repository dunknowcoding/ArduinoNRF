#pragma once

#include <stdbool.h>
#include <stdint.h>

class WatchdogClass {
public:
    bool begin(uint32_t timeoutMs);
    void feed() const;
    bool running() const;
    bool causedReset() const;
};

extern WatchdogClass Watchdog;
