#include "Watchdog.h"

namespace {
constexpr uint32_t WDT_BASE = 0x40010000UL;
constexpr uint32_t WDT_TASKS_START = 0x000UL;
constexpr uint32_t WDT_RUNSTATUS = 0x400UL;
constexpr uint32_t WDT_CRV = 0x504UL;
constexpr uint32_t WDT_RREN = 0x508UL;
constexpr uint32_t WDT_RR0 = 0x600UL;
constexpr uint32_t WDT_RELOAD_MAGIC = 0x6E524635UL;
constexpr uint32_t POWER_BASE = 0x40000000UL;
constexpr uint32_t POWER_RESETREAS = 0x400UL;
constexpr uint32_t RESETREAS_DOG_MASK = 1UL << 1;

inline volatile uint32_t &reg32(uint32_t base, uint32_t offset) {
    return *reinterpret_cast<volatile uint32_t *>(base + offset);
}
}

WatchdogClass Watchdog;

bool WatchdogClass::begin(uint32_t timeoutMs) {
    if (running()) {
        return true;
    }
    if (timeoutMs == 0U) {
        timeoutMs = 1000U;
    }
    reg32(WDT_BASE, WDT_CRV) = (timeoutMs * 32768UL) / 1000UL;
    reg32(WDT_BASE, WDT_RREN) = 1UL;
    reg32(WDT_BASE, WDT_TASKS_START) = 1UL;
    return running();
}

void WatchdogClass::feed() const {
    reg32(WDT_BASE, WDT_RR0) = WDT_RELOAD_MAGIC;
}

bool WatchdogClass::running() const {
    return reg32(WDT_BASE, WDT_RUNSTATUS) != 0UL;
}

bool WatchdogClass::causedReset() const {
    return (reg32(POWER_BASE, POWER_RESETREAS) & RESETREAS_DOG_MASK) != 0UL;
}
