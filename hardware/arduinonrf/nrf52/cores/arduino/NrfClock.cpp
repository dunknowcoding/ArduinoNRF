#include "NrfClock.h"

namespace {
constexpr uint32_t CLOCK_BASE = 0x40000000UL;
constexpr uint32_t CLOCK_TASKS_HFCLKSTART = 0x000UL;
constexpr uint32_t CLOCK_TASKS_LFCLKSTART = 0x008UL;
constexpr uint32_t CLOCK_EVENTS_HFCLKSTARTED = 0x100UL;
constexpr uint32_t CLOCK_EVENTS_LFCLKSTARTED = 0x104UL;
constexpr uint32_t CLOCK_HFCLKSTAT = 0x40CUL;
constexpr uint32_t CLOCK_LFCLKSTAT = 0x418UL;
constexpr uint32_t FICR_BASE = 0x10000000UL;
constexpr uint32_t FICR_DEVICEID0 = 0x060UL;
constexpr uint32_t FICR_DEVICEID1 = 0x064UL;
constexpr uint32_t CLOCK_TIMEOUT_SPINS = 200000UL;

inline volatile uint32_t &reg32(uint32_t base, uint32_t offset) {
    return *reinterpret_cast<volatile uint32_t *>(base + offset);
}
}

void nrfStartLfclk() {
    reg32(CLOCK_BASE, CLOCK_EVENTS_LFCLKSTARTED) = 0UL;
    reg32(CLOCK_BASE, CLOCK_TASKS_LFCLKSTART) = 1UL;
    for (uint32_t spin = 0; spin < CLOCK_TIMEOUT_SPINS; ++spin) {
        if (reg32(CLOCK_BASE, CLOCK_EVENTS_LFCLKSTARTED) != 0UL) {
            break;
        }
    }
}

void nrfStartHfclk() {
    reg32(CLOCK_BASE, CLOCK_EVENTS_HFCLKSTARTED) = 0UL;
    reg32(CLOCK_BASE, CLOCK_TASKS_HFCLKSTART) = 1UL;
    for (uint32_t spin = 0; spin < CLOCK_TIMEOUT_SPINS; ++spin) {
        if (reg32(CLOCK_BASE, CLOCK_EVENTS_HFCLKSTARTED) != 0UL) {
            break;
        }
    }
}

bool nrfLfclkRunning() {
    return (reg32(CLOCK_BASE, CLOCK_LFCLKSTAT) & 0x00010000UL) != 0UL;
}

bool nrfHfclkRunning() {
    return (reg32(CLOCK_BASE, CLOCK_HFCLKSTAT) & 0x00010000UL) != 0UL;
}

uint32_t nrfDeviceIdWord(uint8_t index) {
    if (index == 0U) {
        return reg32(FICR_BASE, FICR_DEVICEID0);
    }
    return reg32(FICR_BASE, FICR_DEVICEID1);
}
