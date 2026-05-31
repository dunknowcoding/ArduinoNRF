#include "NrfClock.h"

namespace {
constexpr uint32_t CLOCK_BASE = 0x40000000UL;
constexpr uint32_t CLOCK_TASKS_HFCLKSTART = 0x000UL;
constexpr uint32_t CLOCK_TASKS_LFCLKSTART = 0x008UL;
constexpr uint32_t CLOCK_TASKS_LFCLKSTOP  = 0x00CUL;
constexpr uint32_t CLOCK_EVENTS_HFCLKSTARTED = 0x100UL;
constexpr uint32_t CLOCK_EVENTS_LFCLKSTARTED = 0x104UL;
constexpr uint32_t CLOCK_HFCLKSTAT = 0x40CUL;
constexpr uint32_t CLOCK_LFCLKSTAT = 0x418UL;
constexpr uint32_t CLOCK_LFCLKSRC  = 0x518UL;
constexpr uint32_t LFCLK_SRC_RC    = 0UL;   // internal RC oscillator
constexpr uint32_t LFCLK_SRC_XTAL  = 1UL;   // external 32.768 kHz crystal (LFXO)
// LFCLKSTAT: bit16 = running, bits[1:0] = source. "running on Xtal" mask/val.
constexpr uint32_t LFCLK_RUNNING   = 0x00010000UL;
constexpr uint32_t LFCLK_SRC_MASK  = 0x00000003UL;
constexpr uint32_t FICR_BASE = 0x10000000UL;
constexpr uint32_t FICR_DEVICEID0 = 0x060UL;
constexpr uint32_t FICR_DEVICEID1 = 0x064UL;
constexpr uint32_t CLOCK_TIMEOUT_SPINS = 200000UL;

inline volatile uint32_t &reg32(uint32_t base, uint32_t offset) {
    return *reinterpret_cast<volatile uint32_t *>(base + offset);
}
}

void nrfStartLfclk() {
    // Prefer the external 32.768 kHz crystal (LFXO). BLE *connections* need an
    // accurate low-frequency clock: the internal RC oscillator is uncalibrated
    // (~250-500 ppm) which is fine for advertising but makes a peripheral miss
    // its connection-event RX windows (verified on hardware - LFCLK was running
    // on RC and connections never established; switching to the crystal fixes
    // the connection-event timing). Accurate LFCLK also benefits RTC / sleepMs.
    //
    // If already running on the crystal, there's nothing to do.
    uint32_t stat = reg32(CLOCK_BASE, CLOCK_LFCLKSTAT);
    if ((stat & LFCLK_RUNNING) && ((stat & LFCLK_SRC_MASK) == LFCLK_SRC_XTAL)) {
        return;
    }

    // Select + start LFXO. The HW runs LFCLK from RC while the crystal warms
    // up and auto-switches when stable (LFCLKSTARTED fires then). Stop first so
    // the source register can be changed.
    reg32(CLOCK_BASE, CLOCK_TASKS_LFCLKSTOP) = 1UL;
    reg32(CLOCK_BASE, CLOCK_LFCLKSRC) = LFCLK_SRC_XTAL;
    reg32(CLOCK_BASE, CLOCK_EVENTS_LFCLKSTARTED) = 0UL;
    reg32(CLOCK_BASE, CLOCK_TASKS_LFCLKSTART) = 1UL;
    // Crystal startup is ~0.25-1 s, so use a generous spin ceiling (the loop
    // exits as soon as LFCLKSTARTED fires - typically well before the ceiling).
    for (uint32_t spin = 0; spin < 60UL * CLOCK_TIMEOUT_SPINS; ++spin) {
        if (reg32(CLOCK_BASE, CLOCK_EVENTS_LFCLKSTARTED) != 0UL) {
            break;
        }
    }

    // Verify it actually came up on the crystal. If not (no xtal populated on
    // this board), fall back to the internal RC so the clock at least runs.
    stat = reg32(CLOCK_BASE, CLOCK_LFCLKSTAT);
    if (!((stat & LFCLK_RUNNING) && ((stat & LFCLK_SRC_MASK) == LFCLK_SRC_XTAL))) {
        reg32(CLOCK_BASE, CLOCK_TASKS_LFCLKSTOP) = 1UL;
        reg32(CLOCK_BASE, CLOCK_LFCLKSRC) = LFCLK_SRC_RC;
        reg32(CLOCK_BASE, CLOCK_EVENTS_LFCLKSTARTED) = 0UL;
        reg32(CLOCK_BASE, CLOCK_TASKS_LFCLKSTART) = 1UL;
        for (uint32_t spin = 0; spin < CLOCK_TIMEOUT_SPINS; ++spin) {
            if (reg32(CLOCK_BASE, CLOCK_EVENTS_LFCLKSTARTED) != 0UL) {
                break;
            }
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
