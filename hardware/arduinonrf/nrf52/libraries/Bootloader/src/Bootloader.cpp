#include "Bootloader.h"

#include "NrfSystem.h"
#include "NrfUsbd.h"
#include "USBDevice.h"

namespace {
constexpr uint32_t POWER_BASE = 0x40000000UL;
constexpr uint32_t POWER_GPREGRET = 0x51CUL;
constexpr uint32_t SCB_AIRCR = 0xE000ED0CUL;
constexpr uint32_t SCB_AIRCR_VECTKEY = 0x05FA0000UL;
constexpr uint32_t SCB_AIRCR_SYSRESETREQ = 1UL << 2;
constexpr uint32_t kUsbDisconnectSpins = 640000UL;

inline volatile uint32_t &reg32(uint32_t base, uint32_t offset) {
    return *reinterpret_cast<volatile uint32_t *>(base + offset);
}

inline volatile uint32_t &mem32(uint32_t address) {
    return *reinterpret_cast<volatile uint32_t *>(address);
}
}

BootloaderClass Bootloader;

void BootloaderClass::markForResetToBootloader() const {
    markForResetToBootloader(nrfBootloaderUploadResetMagic());
}

void BootloaderClass::markForResetToBootloader(uint8_t marker) const {
    nrfPrepareBootloaderResetRequest(marker);
}

bool BootloaderClass::resetRequested() const {
    return requestMarker() != 0U;
}

uint8_t BootloaderClass::requestMarker() const {
    return static_cast<uint8_t>(reg32(POWER_BASE, POWER_GPREGRET) & 0xFFUL);
}

void BootloaderClass::clearRequest() const {
    nrfClearBootloaderResetRequest();
}

void BootloaderClass::resetToBootloader() const {
    resetToBootloader(nrfBootloaderUploadResetMagic());
}

void BootloaderClass::resetToBootloader(uint8_t marker) const {
    USBDevice.detach();
    nrfUsbdDriver().end();
    for (volatile uint32_t spin = 0UL; spin < kUsbDisconnectSpins; ++spin) {
        __asm volatile("nop");
    }
    markForResetToBootloader(marker);
    __asm volatile("dsb 0xF" ::: "memory");
    __asm volatile("isb 0xF" ::: "memory");
    mem32(SCB_AIRCR) = SCB_AIRCR_VECTKEY | SCB_AIRCR_SYSRESETREQ;
    __asm volatile("dsb 0xF" ::: "memory");
    while (true) {
    }
}
