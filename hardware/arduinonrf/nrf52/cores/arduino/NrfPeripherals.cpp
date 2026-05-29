// NrfPeripherals.cpp - implementation of the small drivers in
// NrfPeripherals.h. Designed to be standalone (no Arduino.h includes) so
// these can be reused outside the core too.

#include "NrfPeripherals.h"
#include "NrfClock.h"
#include <math.h>   // NAN

namespace {

inline volatile uint32_t &reg32(uint32_t base, uint32_t offset) {
    return *reinterpret_cast<volatile uint32_t *>(base + offset);
}

// ---- shared register definitions -----------------------------------------

constexpr uint32_t RNG_BASE = 0x4000D000UL;
constexpr uint32_t RNG_TASKS_START   = 0x000UL;
constexpr uint32_t RNG_TASKS_STOP    = 0x004UL;
constexpr uint32_t RNG_EVENTS_VALRDY = 0x100UL;
constexpr uint32_t RNG_CONFIG        = 0x504UL;
constexpr uint32_t RNG_VALUE         = 0x508UL;
constexpr uint32_t RNG_CONFIG_DERCEN = 1UL << 0;

constexpr uint32_t WDT_BASE = 0x40010000UL;
constexpr uint32_t WDT_TASKS_START = 0x000UL;
constexpr uint32_t WDT_EVENTS_TIMEOUT = 0x100UL;
constexpr uint32_t WDT_RUNSTATUS  = 0x400UL;
constexpr uint32_t WDT_REQSTATUS  = 0x404UL;
constexpr uint32_t WDT_CRV        = 0x504UL;
constexpr uint32_t WDT_RREN       = 0x508UL;
constexpr uint32_t WDT_CONFIG     = 0x50CUL;
constexpr uint32_t WDT_RR_BASE    = 0x600UL;        // RR[0..7] @ +i*4
constexpr uint32_t WDT_RR_RELOAD_MAGIC = 0x6E524635UL;  // "nRF5" — required to feed

constexpr uint32_t TEMP_BASE = 0x4000C000UL;
constexpr uint32_t TEMP_TASKS_START   = 0x000UL;
constexpr uint32_t TEMP_TASKS_STOP    = 0x004UL;
constexpr uint32_t TEMP_EVENTS_DATARDY = 0x100UL;
constexpr uint32_t TEMP_TEMP          = 0x508UL;

constexpr uint32_t QDEC_BASE = 0x40012000UL;
constexpr uint32_t QDEC_TASKS_START   = 0x000UL;
constexpr uint32_t QDEC_TASKS_STOP    = 0x004UL;
constexpr uint32_t QDEC_TASKS_READCLRACC = 0x008UL;
constexpr uint32_t QDEC_TASKS_RDCLRACC = 0x00CUL;
constexpr uint32_t QDEC_ENABLE        = 0x500UL;
constexpr uint32_t QDEC_LEDPOL        = 0x504UL;
constexpr uint32_t QDEC_SAMPLEPER     = 0x508UL;
constexpr uint32_t QDEC_SAMPLE        = 0x50CUL;
constexpr uint32_t QDEC_REPORTPER     = 0x510UL;
constexpr uint32_t QDEC_ACC           = 0x514UL;
constexpr uint32_t QDEC_ACCREAD       = 0x518UL;
constexpr uint32_t QDEC_PSEL_LED      = 0x51CUL;
constexpr uint32_t QDEC_PSEL_A        = 0x520UL;
constexpr uint32_t QDEC_PSEL_B        = 0x524UL;
constexpr uint32_t QDEC_DBFEN         = 0x528UL;
constexpr uint32_t QDEC_LEDPRE        = 0x540UL;
constexpr uint32_t QDEC_ACCDBL        = 0x544UL;
constexpr uint32_t QDEC_ACCDBLREAD    = 0x548UL;

// POWER.RESETREAS for "watchdog reset" detection
constexpr uint32_t POWER_BASE = 0x40000000UL;
constexpr uint32_t POWER_RESETREAS = 0x400UL;
constexpr uint32_t RESETREAS_DOG_BIT = 1UL << 1;

bool g_wdtRunning = false;
bool g_qdecRunning = false;
bool g_rngRunning = false;

}  // namespace

// ============================================================================
// NrfRng
// ============================================================================

void NrfRng::begin() {
    reg32(RNG_BASE, RNG_CONFIG) = RNG_CONFIG_DERCEN;
    reg32(RNG_BASE, RNG_EVENTS_VALRDY) = 0UL;
    reg32(RNG_BASE, RNG_TASKS_START) = 1UL;
    g_rngRunning = true;
}

void NrfRng::end() {
    reg32(RNG_BASE, RNG_TASKS_STOP) = 1UL;
    g_rngRunning = false;
}

bool NrfRng::isRunning() { return g_rngRunning; }

void NrfRng::randomBytes(uint8_t *buf, size_t len) {
    if (!g_rngRunning) {
        begin();
    }
    for (size_t i = 0; i < len; ++i) {
        // Wait for VALRDY then read one byte. The bias corrector means each
        // VALRDY may take up to ~120 us; typical ~5 us.
        while (reg32(RNG_BASE, RNG_EVENTS_VALRDY) == 0UL) {}
        buf[i] = static_cast<uint8_t>(reg32(RNG_BASE, RNG_VALUE) & 0xFFUL);
        reg32(RNG_BASE, RNG_EVENTS_VALRDY) = 0UL;
    }
}

uint32_t NrfRng::random32() {
    uint32_t v = 0;
    randomBytes(reinterpret_cast<uint8_t *>(&v), 4U);
    return v;
}

// ============================================================================
// NrfWdt
// ============================================================================

bool NrfWdt::begin(uint32_t timeoutMs, uint8_t channelMask, bool pauseOnHalt) {
    if (g_wdtRunning) {
        // WDT is run-locked once started - we cannot reconfigure.
        return false;
    }
    if (channelMask == 0U) return false;

    // Make sure LFCLK is up.
    if (!nrfLfclkRunning()) {
        nrfStartLfclk();
    }

    // CONFIG: bit 0 = SLEEP (1 = WDT keeps ticking during sleep),
    //         bit 3 = HALT (1 = pause while CPU halted in debug).
    uint32_t config = 1UL;        // run during sleep (we want it to)
    if (pauseOnHalt) config |= (1UL << 3);
    reg32(WDT_BASE, WDT_CONFIG) = config;

    // CRV: counter-reload value, in LFCLK ticks (32768 Hz). For X ms:
    //   ticks = (X * 32768) / 1000
    const uint64_t ticks = (static_cast<uint64_t>(timeoutMs) * 32768ULL) / 1000ULL;
    if (ticks < 1ULL) return false;
    if (ticks > 0xFFFFFFFFULL) return false;
    reg32(WDT_BASE, WDT_CRV) = static_cast<uint32_t>(ticks);

    // RREN: which RR channels participate.
    reg32(WDT_BASE, WDT_RREN) = channelMask;

    reg32(WDT_BASE, WDT_TASKS_START) = 1UL;
    g_wdtRunning = true;
    return true;
}

void NrfWdt::feed(uint8_t channel) {
    if (channel >= MAX_RELOAD_CHANNELS) return;
    reg32(WDT_BASE, WDT_RR_BASE + (channel * 4UL)) = WDT_RR_RELOAD_MAGIC;
}

bool NrfWdt::isRunning() {
    return (reg32(WDT_BASE, WDT_RUNSTATUS) & 1UL) != 0UL;
}

bool NrfWdt::causedLastReset() {
    return (reg32(POWER_BASE, POWER_RESETREAS) & RESETREAS_DOG_BIT) != 0UL;
}

// ============================================================================
// NrfTemp
// ============================================================================

bool NrfTemp::readRaw(int32_t *outQuartersC) {
    if (outQuartersC == nullptr) return false;
    reg32(TEMP_BASE, TEMP_EVENTS_DATARDY) = 0UL;
    reg32(TEMP_BASE, TEMP_TASKS_START) = 1UL;
    // Conversion takes ~36 µs. Spin with a generous timeout.
    for (uint32_t spin = 0; spin < 1000000UL; ++spin) {
        if (reg32(TEMP_BASE, TEMP_EVENTS_DATARDY) != 0UL) {
            *outQuartersC = static_cast<int32_t>(reg32(TEMP_BASE, TEMP_TEMP));
            reg32(TEMP_BASE, TEMP_TASKS_STOP) = 1UL;
            return true;
        }
    }
    reg32(TEMP_BASE, TEMP_TASKS_STOP) = 1UL;
    return false;
}

float NrfTemp::readCelsius() {
    int32_t raw = 0;
    if (!readRaw(&raw)) return NAN;
    return static_cast<float>(raw) * 0.25f;
}

float NrfTemp::readFahrenheit() {
    const float c = readCelsius();
    if (isnan(c)) return NAN;
    return c * (9.0f / 5.0f) + 32.0f;
}

// ============================================================================
// NrfQdec
// ============================================================================

bool NrfQdec::begin(uint8_t pinA, uint8_t pinB, uint8_t pinLed, uint16_t sampleRateHz) {
    if (g_qdecRunning) return false;

    // SAMPLEPER picks the divider for the 32-kHz sampling clock.
    // Values: 0=128us, 1=256us, 2=512us, 3=1024us, 4=2048us, ...
    uint32_t samplePer = 3UL;  // 1024 µs ≈ ~1 kHz default
    switch (sampleRateHz) {
        case 8000: samplePer = 0; break;   // 128 µs
        case 4000: samplePer = 1; break;   // 256 µs
        case 2000: samplePer = 2; break;   // 512 µs
        case 1000: samplePer = 3; break;
        case  500: samplePer = 4; break;
        case  250: samplePer = 5; break;
        case  125: samplePer = 6; break;
        case   64: samplePer = 7; break;
        default: break;
    }

    reg32(QDEC_BASE, QDEC_SAMPLEPER) = samplePer;
    reg32(QDEC_BASE, QDEC_PSEL_A) = pinA;
    reg32(QDEC_BASE, QDEC_PSEL_B) = pinB;
    reg32(QDEC_BASE, QDEC_PSEL_LED) = (pinLed == 0xFFU) ? 0xFFFFFFFFUL : pinLed;
    reg32(QDEC_BASE, QDEC_DBFEN) = 1UL;          // input debouncing on
    reg32(QDEC_BASE, QDEC_REPORTPER) = 0UL;       // 10 samples per report
    reg32(QDEC_BASE, QDEC_ENABLE) = 1UL;
    reg32(QDEC_BASE, QDEC_TASKS_START) = 1UL;
    g_qdecRunning = true;
    return true;
}

void NrfQdec::end() {
    reg32(QDEC_BASE, QDEC_TASKS_STOP) = 1UL;
    reg32(QDEC_BASE, QDEC_ENABLE) = 0UL;
    g_qdecRunning = false;
}

int32_t NrfQdec::readDelta() {
    if (!g_qdecRunning) return 0;
    reg32(QDEC_BASE, QDEC_TASKS_RDCLRACC) = 1UL;
    return static_cast<int32_t>(reg32(QDEC_BASE, QDEC_ACCREAD));
}

int32_t NrfQdec::readPosition() {
    if (!g_qdecRunning) return 0;
    return static_cast<int32_t>(reg32(QDEC_BASE, QDEC_ACC));
}

void NrfQdec::resetPosition() {
    if (!g_qdecRunning) return;
    reg32(QDEC_BASE, QDEC_TASKS_RDCLRACC) = 1UL;
    // discard the read
    (void)reg32(QDEC_BASE, QDEC_ACCREAD);
}

bool NrfQdec::isRunning() { return g_qdecRunning; }
