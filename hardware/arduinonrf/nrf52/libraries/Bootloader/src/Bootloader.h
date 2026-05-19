#pragma once

#include <stdint.h>

class BootloaderClass {
public:
    void markForResetToBootloader() const;
    void markForResetToBootloader(uint8_t marker) const;
    bool resetRequested() const;
    uint8_t requestMarker() const;
    void clearRequest() const;
    void resetToBootloader() const;
    void resetToBootloader(uint8_t marker) const;
};

extern BootloaderClass Bootloader;
