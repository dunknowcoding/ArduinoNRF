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

constexpr uint32_t NVIC_BASE  = 0xE000E000UL;
constexpr uint32_t NVIC_ISER0 = 0x100UL;
constexpr uint32_t NVIC_ICER0 = 0x180UL;

inline void enableNvic(uint8_t irq) {
    reg32(NVIC_BASE, NVIC_ISER0 + ((irq >> 5U) * 4UL)) = 1UL << (irq & 0x1FU);
}
inline void disableNvic(uint8_t irq) {
    reg32(NVIC_BASE, NVIC_ICER0 + ((irq >> 5U) * 4UL)) = 1UL << (irq & 0x1FU);
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
    // RDCLRACC captures ACC into ACCREAD and clears ACC in one shot; the
    // clear is done by the task, so there's nothing more to read here.
    reg32(QDEC_BASE, QDEC_TASKS_RDCLRACC) = 1UL;
}

bool NrfQdec::isRunning() { return g_qdecRunning; }

// ============================================================================
// NrfTimer (TIMER0..TIMER4)
// ============================================================================

namespace {
constexpr uint32_t TIMER0_BASE = 0x40008000UL;
constexpr uint32_t TIMER1_BASE = 0x40009000UL;
constexpr uint32_t TIMER2_BASE = 0x4000A000UL;
constexpr uint32_t TIMER3_BASE = 0x4001A000UL;
constexpr uint32_t TIMER4_BASE = 0x4001B000UL;

constexpr uint32_t TIMER_TASKS_START   = 0x000UL;
constexpr uint32_t TIMER_TASKS_STOP    = 0x004UL;
constexpr uint32_t TIMER_TASKS_CLEAR   = 0x00CUL;
constexpr uint32_t TIMER_TASKS_CAPTURE = 0x040UL;   // +cc*4
constexpr uint32_t TIMER_EVENTS_COMPARE = 0x140UL;  // +cc*4
constexpr uint32_t TIMER_INTENSET      = 0x304UL;
constexpr uint32_t TIMER_INTENCLR      = 0x308UL;
constexpr uint32_t TIMER_MODE          = 0x504UL;
constexpr uint32_t TIMER_BITMODE       = 0x508UL;
constexpr uint32_t TIMER_PRESCALER     = 0x510UL;
constexpr uint32_t TIMER_CC            = 0x540UL;   // CC[0..5] @ +cc*4

constexpr uint32_t TIMER_INTEN_COMPARE_BIT0 = 1UL << 16;
constexpr uint32_t TIMER_BITMODE_32 = 3UL;
constexpr uint32_t TIMER_BITMODE_16 = 0UL;
constexpr uint32_t TIMER_BASE_CLOCK_HZ = 16000000UL;

constexpr uint8_t TIMER0_IRQ = 8U;
constexpr uint8_t TIMER1_IRQ = 9U;
constexpr uint8_t TIMER2_IRQ = 10U;
constexpr uint8_t TIMER3_IRQ = 26U;
constexpr uint8_t TIMER4_IRQ = 27U;

uint32_t timerBaseForIndex(uint8_t index) {
    switch (index) {
        case 0: return TIMER0_BASE;
        case 1: return TIMER1_BASE;
        case 2: return TIMER2_BASE;
        case 3: return TIMER3_BASE;
        case 4: return TIMER4_BASE;
        default: return 0U;
    }
}
uint8_t timerIrqForIndex(uint8_t index) {
    switch (index) {
        case 0: return TIMER0_IRQ;
        case 1: return TIMER1_IRQ;
        case 2: return TIMER2_IRQ;
        case 3: return TIMER3_IRQ;
        case 4: return TIMER4_IRQ;
        default: return 0xFFU;
    }
}
}  // namespace

NrfTimer::NrfTimer(uint8_t index)
    : base_(timerBaseForIndex(index)),
      index_(index),
      irqNumber_(timerIrqForIndex(index)),
      tickHz_(0UL) {
    for (uint8_t i = 0; i < COMPARE_CHANNEL_COUNT; ++i) {
        compareCallbacks_[i] = nullptr;
    }
}

bool NrfTimer::begin(uint32_t tickHz, bool narrow16) {
    if (!isValid() || tickHz == 0UL || tickHz > TIMER_BASE_CLOCK_HZ) {
        return false;
    }
    // Pick the smallest prescaler P with (16 MHz >> P) <= tickHz desired
    // resolution. With P=0 the counter ticks at 16 MHz; P=9 at 31.25 kHz.
    uint32_t prescaler = 0;
    uint32_t clockHz = TIMER_BASE_CLOCK_HZ;
    while (prescaler < 9U && clockHz > tickHz) {
        ++prescaler;
        clockHz >>= 1;
    }

    reg32(base_, TIMER_TASKS_STOP) = 1UL;
    reg32(base_, TIMER_TASKS_CLEAR) = 1UL;
    reg32(base_, TIMER_MODE) = 0UL;   // Timer mode (not Counter)
    reg32(base_, TIMER_BITMODE) = narrow16 ? TIMER_BITMODE_16 : TIMER_BITMODE_32;
    reg32(base_, TIMER_PRESCALER) = prescaler;
    reg32(base_, TIMER_INTENCLR) = 0xFFFFFFFFUL;
    for (uint8_t i = 0; i < COMPARE_CHANNEL_COUNT; ++i) {
        reg32(base_, TIMER_EVENTS_COMPARE + (i * 4U)) = 0UL;
    }
    tickHz_ = clockHz;
    return true;
}

void NrfTimer::end() {
    if (!isValid()) return;
    reg32(base_, TIMER_TASKS_STOP) = 1UL;
    reg32(base_, TIMER_INTENCLR) = 0xFFFFFFFFUL;
    disableNvic(irqNumber_);
    tickHz_ = 0UL;
    for (uint8_t i = 0; i < COMPARE_CHANNEL_COUNT; ++i) {
        compareCallbacks_[i] = nullptr;
    }
}

void NrfTimer::start() { if (isValid()) reg32(base_, TIMER_TASKS_START) = 1UL; }
void NrfTimer::stop()  { if (isValid()) reg32(base_, TIMER_TASKS_STOP)  = 1UL; }
void NrfTimer::clear() { if (isValid()) reg32(base_, TIMER_TASKS_CLEAR) = 1UL; }

uint32_t NrfTimer::counter() {
    if (!isValid()) return 0UL;
    // CAPTURE[5] -> read CC[5] - the convention for "read current count".
    reg32(base_, TIMER_TASKS_CAPTURE + (5U * 4U)) = 1UL;
    return reg32(base_, TIMER_CC + (5U * 4U));
}

void NrfTimer::setCompare(uint8_t cc, uint32_t value) {
    if (!isValid() || cc >= COMPARE_CHANNEL_COUNT) return;
    reg32(base_, TIMER_CC + (cc * 4U)) = value;
}

uint32_t NrfTimer::getCompare(uint8_t cc) const {
    if (!isValid() || cc >= COMPARE_CHANNEL_COUNT) return 0UL;
    return *reinterpret_cast<volatile const uint32_t *>(base_ + TIMER_CC + (cc * 4U));
}

void NrfTimer::attachCompareInterrupt(uint8_t cc, nrfTimerCallback_t cb) {
    if (!isValid() || cc >= COMPARE_CHANNEL_COUNT) return;
    compareCallbacks_[cc] = cb;
    if (cb != nullptr) {
        reg32(base_, TIMER_EVENTS_COMPARE + (cc * 4U)) = 0UL;
        reg32(base_, TIMER_INTENSET) = TIMER_INTEN_COMPARE_BIT0 << cc;
        enableNvic(irqNumber_);
    } else {
        reg32(base_, TIMER_INTENCLR) = TIMER_INTEN_COMPARE_BIT0 << cc;
    }
}

void NrfTimer::detachCompareInterrupt(uint8_t cc) {
    attachCompareInterrupt(cc, nullptr);
}

uint32_t NrfTimer::taskCaptureAddr(uint8_t cc) const {
    if (!isValid() || cc >= COMPARE_CHANNEL_COUNT) return 0UL;
    return base_ + TIMER_TASKS_CAPTURE + (cc * 4U);
}

uint32_t NrfTimer::eventCompareAddr(uint8_t cc) const {
    if (!isValid() || cc >= COMPARE_CHANNEL_COUNT) return 0UL;
    return base_ + TIMER_EVENTS_COMPARE + (cc * 4U);
}

uint32_t NrfTimer::taskStartAddr() const { return isValid() ? base_ + TIMER_TASKS_START : 0UL; }
uint32_t NrfTimer::taskStopAddr()  const { return isValid() ? base_ + TIMER_TASKS_STOP  : 0UL; }

void NrfTimer::serviceIrq() {
    if (!isValid()) return;
    for (uint8_t cc = 0; cc < COMPARE_CHANNEL_COUNT; ++cc) {
        const uint32_t off = TIMER_EVENTS_COMPARE + (cc * 4U);
        if (reg32(base_, off) != 0UL) {
            reg32(base_, off) = 0UL;
            if (compareCallbacks_[cc] != nullptr) {
                compareCallbacks_[cc]();
            }
        }
    }
}

NrfTimer &nrfTimer0() { static NrfTimer t(0); return t; }
NrfTimer &nrfTimer1() { static NrfTimer t(1); return t; }
NrfTimer &nrfTimer2() { static NrfTimer t(2); return t; }
NrfTimer &nrfTimer3() { static NrfTimer t(3); return t; }
NrfTimer &nrfTimer4() { static NrfTimer t(4); return t; }

extern "C" {
void TIMER0_IRQHandler(void) { nrfTimer0().serviceIrq(); }
void TIMER1_IRQHandler(void) { nrfTimer1().serviceIrq(); }
void TIMER2_IRQHandler(void) { nrfTimer2().serviceIrq(); }
void TIMER3_IRQHandler(void) { nrfTimer3().serviceIrq(); }
void TIMER4_IRQHandler(void) { nrfTimer4().serviceIrq(); }
}

// ============================================================================
// NrfNvmc - direct flash erase / write
// ============================================================================

namespace {
constexpr uint32_t NVMC_BASE = 0x4001E000UL;
constexpr uint32_t NVMC_READY     = 0x400UL;
constexpr uint32_t NVMC_CONFIG    = 0x504UL;
constexpr uint32_t NVMC_ERASEPAGE = 0x508UL;
constexpr uint32_t NVMC_ERASEALL  = 0x50CUL;
constexpr uint32_t NVMC_ERASEUICR = 0x514UL;

constexpr uint32_t NVMC_CONFIG_REN = 0UL;
constexpr uint32_t NVMC_CONFIG_WEN = 1UL;
constexpr uint32_t NVMC_CONFIG_EEN = 2UL;

constexpr uint32_t NRF_FLASH_BASE = 0x00000000UL;
constexpr uint32_t NRF_FLASH_SIZE = 0x00100000UL;   // 1 MB on nRF52840
}

bool NrfNvmc::isReady() {
    return (reg32(NVMC_BASE, NVMC_READY) & 1UL) != 0UL;
}

bool NrfNvmc::erasePage(uint32_t flashAddress) {
    if (flashAddress < NRF_FLASH_BASE || flashAddress >= NRF_FLASH_BASE + NRF_FLASH_SIZE) {
        return false;
    }
    if ((flashAddress & (PAGE_SIZE_BYTES - 1U)) != 0U) {
        return false;   // page-misaligned
    }
    while (!isReady()) {}
    reg32(NVMC_BASE, NVMC_CONFIG) = NVMC_CONFIG_EEN;
    while (!isReady()) {}
    reg32(NVMC_BASE, NVMC_ERASEPAGE) = flashAddress;
    while (!isReady()) {}
    reg32(NVMC_BASE, NVMC_CONFIG) = NVMC_CONFIG_REN;
    return true;
}

bool NrfNvmc::eraseRegion(uint32_t flashAddress, uint32_t lengthBytes) {
    if (lengthBytes == 0U) return true;
    if ((flashAddress & (PAGE_SIZE_BYTES - 1U)) != 0U) return false;
    const uint32_t endAddr = flashAddress + lengthBytes;
    for (uint32_t addr = flashAddress; addr < endAddr; addr += PAGE_SIZE_BYTES) {
        if (!erasePage(addr)) return false;
    }
    return true;
}

bool NrfNvmc::writeWord(uint32_t flashAddress, uint32_t word) {
    if ((flashAddress & 0x3U) != 0U) return false;
    if (flashAddress < NRF_FLASH_BASE || flashAddress >= NRF_FLASH_BASE + NRF_FLASH_SIZE) {
        return false;
    }
    while (!isReady()) {}
    reg32(NVMC_BASE, NVMC_CONFIG) = NVMC_CONFIG_WEN;
    while (!isReady()) {}
    *reinterpret_cast<volatile uint32_t *>(flashAddress) = word;
    while (!isReady()) {}
    reg32(NVMC_BASE, NVMC_CONFIG) = NVMC_CONFIG_REN;
    return true;
}

bool NrfNvmc::writeWords(uint32_t flashAddress, const uint32_t *words, uint32_t wordCount) {
    if (words == nullptr) return false;
    for (uint32_t i = 0; i < wordCount; ++i) {
        if (!writeWord(flashAddress + (i * 4U), words[i])) {
            return false;
        }
    }
    return true;
}

bool NrfNvmc::writeBytes(uint32_t flashAddress, const uint8_t *bytes, size_t length) {
    if (bytes == nullptr) return false;
    if ((flashAddress & 0x3U) != 0U) return false;
    while (length >= 4U) {
        uint32_t w = static_cast<uint32_t>(bytes[0])        |
                     (static_cast<uint32_t>(bytes[1]) << 8) |
                     (static_cast<uint32_t>(bytes[2]) << 16) |
                     (static_cast<uint32_t>(bytes[3]) << 24);
        if (!writeWord(flashAddress, w)) return false;
        flashAddress += 4U;
        bytes += 4U;
        length -= 4U;
    }
    if (length > 0U) {
        // Tail: pack remaining bytes into a word with high bytes as 0xFF
        // (so unwritten bits stay erased).
        uint32_t w = 0xFFFFFFFFUL;
        for (size_t i = 0; i < length; ++i) {
            w = (w & ~(0xFFUL << (i * 8U))) | (static_cast<uint32_t>(bytes[i]) << (i * 8U));
        }
        if (!writeWord(flashAddress, w)) return false;
    }
    return true;
}

// ============================================================================
// NrfPpi - peripheral interconnect
// ============================================================================

namespace {
constexpr uint32_t PPI_BASE = 0x4001F000UL;
constexpr uint32_t PPI_CHEN     = 0x500UL;
constexpr uint32_t PPI_CHENSET  = 0x504UL;
constexpr uint32_t PPI_CHENCLR  = 0x508UL;
constexpr uint32_t PPI_CH_EEP_BASE = 0x510UL;   // CH[i].EEP @ +i*8
constexpr uint32_t PPI_CH_TEP_BASE = 0x514UL;   // CH[i].TEP @ +i*8

uint32_t g_ppiAllocated = 0;   // bit i = channel i in use
}

uint8_t NrfPpi::allocateChannel() {
    for (uint8_t ch = 0; ch < USER_CHANNEL_COUNT; ++ch) {
        if ((g_ppiAllocated & (1UL << ch)) == 0UL) {
            g_ppiAllocated |= (1UL << ch);
            return ch;
        }
    }
    return INVALID_CHANNEL;
}

void NrfPpi::releaseChannel(uint8_t channel) {
    if (channel >= USER_CHANNEL_COUNT) return;
    disable(channel);
    g_ppiAllocated &= ~(1UL << channel);
}

bool NrfPpi::configure(uint8_t channel, uint32_t eventAddr, uint32_t taskAddr) {
    if (channel >= USER_CHANNEL_COUNT) return false;
    if (eventAddr == 0UL || taskAddr == 0UL) return false;
    reg32(PPI_BASE, PPI_CH_EEP_BASE + (channel * 8UL)) = eventAddr;
    reg32(PPI_BASE, PPI_CH_TEP_BASE + (channel * 8UL)) = taskAddr;
    return true;
}

void NrfPpi::enable(uint8_t channel) {
    if (channel >= USER_CHANNEL_COUNT) return;
    reg32(PPI_BASE, PPI_CHENSET) = 1UL << channel;
}

void NrfPpi::disable(uint8_t channel) {
    if (channel >= USER_CHANNEL_COUNT) return;
    reg32(PPI_BASE, PPI_CHENCLR) = 1UL << channel;
}

bool NrfPpi::isEnabled(uint8_t channel) {
    if (channel >= USER_CHANNEL_COUNT) return false;
    return (reg32(PPI_BASE, PPI_CHEN) & (1UL << channel)) != 0UL;
}

uint8_t NrfPpi::wire(uint32_t eventAddr, uint32_t taskAddr) {
    const uint8_t ch = allocateChannel();
    if (ch == INVALID_CHANNEL) return INVALID_CHANNEL;
    if (!configure(ch, eventAddr, taskAddr)) {
        releaseChannel(ch);
        return INVALID_CHANNEL;
    }
    enable(ch);
    return ch;
}

// ============================================================================
// NrfEgu (EGU0..EGU5)
// ============================================================================

namespace {
constexpr uint32_t EGU_BASE_LIST[6] = {
    0x40014000UL, 0x40015000UL, 0x40016000UL,
    0x40017000UL, 0x40018000UL, 0x40019000UL,
};
constexpr uint8_t EGU_IRQ_LIST[6] = { 20, 21, 22, 23, 24, 25 };

constexpr uint32_t EGU_TASKS_TRIGGER     = 0x000UL;   // +ch*4
constexpr uint32_t EGU_EVENTS_TRIGGERED  = 0x100UL;   // +ch*4
constexpr uint32_t EGU_INTENSET          = 0x304UL;
constexpr uint32_t EGU_INTENCLR          = 0x308UL;
}

NrfEgu::NrfEgu(uint8_t index)
    : base_(index < 6 ? EGU_BASE_LIST[index] : 0U),
      index_(index),
      irqNumber_(index < 6 ? EGU_IRQ_LIST[index] : 0xFFU) {
    for (uint8_t i = 0; i < CHANNEL_COUNT; ++i) callbacks_[i] = nullptr;
}

void NrfEgu::trigger(uint8_t channel) {
    if (!isValid() || channel >= CHANNEL_COUNT) return;
    reg32(base_, EGU_TASKS_TRIGGER + (channel * 4U)) = 1UL;
}

void NrfEgu::attachInterrupt(uint8_t channel, nrfEguCallback_t cb) {
    if (!isValid() || channel >= CHANNEL_COUNT) return;
    callbacks_[channel] = cb;
    if (cb != nullptr) {
        reg32(base_, EGU_EVENTS_TRIGGERED + (channel * 4U)) = 0UL;
        reg32(base_, EGU_INTENSET) = 1UL << channel;
        enableNvic(irqNumber_);
    } else {
        reg32(base_, EGU_INTENCLR) = 1UL << channel;
    }
}

void NrfEgu::detachInterrupt(uint8_t channel) { attachInterrupt(channel, nullptr); }

uint32_t NrfEgu::taskTriggerAddr(uint8_t channel) const {
    if (!isValid() || channel >= CHANNEL_COUNT) return 0UL;
    return base_ + EGU_TASKS_TRIGGER + (channel * 4U);
}
uint32_t NrfEgu::eventTriggeredAddr(uint8_t channel) const {
    if (!isValid() || channel >= CHANNEL_COUNT) return 0UL;
    return base_ + EGU_EVENTS_TRIGGERED + (channel * 4U);
}

void NrfEgu::serviceIrq() {
    if (!isValid()) return;
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
        const uint32_t off = EGU_EVENTS_TRIGGERED + (ch * 4U);
        if (reg32(base_, off) != 0UL) {
            reg32(base_, off) = 0UL;
            if (callbacks_[ch] != nullptr) callbacks_[ch]();
        }
    }
}

NrfEgu &nrfEgu0() { static NrfEgu e(0); return e; }
NrfEgu &nrfEgu1() { static NrfEgu e(1); return e; }
NrfEgu &nrfEgu2() { static NrfEgu e(2); return e; }
NrfEgu &nrfEgu3() { static NrfEgu e(3); return e; }
NrfEgu &nrfEgu4() { static NrfEgu e(4); return e; }
NrfEgu &nrfEgu5() { static NrfEgu e(5); return e; }

extern "C" {
void SWI0_EGU0_IRQHandler(void) { nrfEgu0().serviceIrq(); }
void SWI1_EGU1_IRQHandler(void) { nrfEgu1().serviceIrq(); }
void SWI2_EGU2_IRQHandler(void) { nrfEgu2().serviceIrq(); }
void SWI3_EGU3_IRQHandler(void) { nrfEgu3().serviceIrq(); }
void SWI4_EGU4_IRQHandler(void) { nrfEgu4().serviceIrq(); }
void SWI5_EGU5_IRQHandler(void) { nrfEgu5().serviceIrq(); }
}

// ============================================================================
// NrfComp - analog comparator
// ============================================================================

namespace {
constexpr uint32_t COMP_BASE = 0x40013000UL;
constexpr uint32_t COMP_TASKS_START   = 0x000UL;
constexpr uint32_t COMP_TASKS_STOP    = 0x004UL;
constexpr uint32_t COMP_TASKS_SAMPLE  = 0x008UL;
constexpr uint32_t COMP_EVENTS_READY  = 0x100UL;
constexpr uint32_t COMP_EVENTS_DOWN   = 0x104UL;
constexpr uint32_t COMP_EVENTS_UP     = 0x108UL;
constexpr uint32_t COMP_EVENTS_CROSS  = 0x10CUL;
constexpr uint32_t COMP_INTENSET      = 0x304UL;
constexpr uint32_t COMP_INTENCLR      = 0x308UL;
constexpr uint32_t COMP_RESULT        = 0x400UL;
constexpr uint32_t COMP_ENABLE        = 0x500UL;
constexpr uint32_t COMP_PSEL          = 0x504UL;
constexpr uint32_t COMP_REFSEL        = 0x508UL;
constexpr uint32_t COMP_EXTREFSEL     = 0x50CUL;
constexpr uint32_t COMP_TH            = 0x530UL;
constexpr uint32_t COMP_MODE          = 0x534UL;
constexpr uint32_t COMP_HYST          = 0x538UL;

constexpr uint8_t COMP_IRQ = 19U;   // COMP_LPCOMP_IRQHandler

nrfCompCallback_t g_compCallback = nullptr;
}

bool NrfComp::begin(InputPin input, Reference ref) {
    reg32(COMP_BASE, COMP_ENABLE) = 0UL;
    reg32(COMP_BASE, COMP_PSEL) = static_cast<uint32_t>(input);
    reg32(COMP_BASE, COMP_REFSEL) = static_cast<uint32_t>(ref);
    reg32(COMP_BASE, COMP_TH) = (0x10UL << 8) | 0x10UL;   // VUP=VDOWN, middle threshold
    reg32(COMP_BASE, COMP_MODE) = 0UL;     // single-ended
    reg32(COMP_BASE, COMP_HYST) = 0UL;
    reg32(COMP_BASE, COMP_ENABLE) = 1UL;

    reg32(COMP_BASE, COMP_EVENTS_READY) = 0UL;
    reg32(COMP_BASE, COMP_TASKS_START) = 1UL;
    // Wait for READY.
    for (uint32_t spin = 0; spin < 100000UL; ++spin) {
        if (reg32(COMP_BASE, COMP_EVENTS_READY) != 0UL) return true;
    }
    return false;
}

void NrfComp::end() {
    reg32(COMP_BASE, COMP_TASKS_STOP) = 1UL;
    reg32(COMP_BASE, COMP_ENABLE) = 0UL;
    reg32(COMP_BASE, COMP_INTENCLR) = 0xFFFFFFFFUL;
    disableNvic(COMP_IRQ);
    g_compCallback = nullptr;
}

bool NrfComp::isAbove() {
    reg32(COMP_BASE, COMP_TASKS_SAMPLE) = 1UL;
    return (reg32(COMP_BASE, COMP_RESULT) & 1UL) != 0UL;
}

void NrfComp::attachInterrupt(Mode mode, nrfCompCallback_t cb) {
    g_compCallback = cb;
    reg32(COMP_BASE, COMP_INTENCLR) = 0xFFFFFFFFUL;
    if (cb == nullptr) return;
    reg32(COMP_BASE, COMP_EVENTS_DOWN) = 0UL;
    reg32(COMP_BASE, COMP_EVENTS_UP)   = 0UL;
    reg32(COMP_BASE, COMP_EVENTS_CROSS) = 0UL;
    uint32_t inten = 0UL;
    switch (mode) {
        case MODE_UP:    inten = 1UL << 2; break;
        case MODE_DOWN:  inten = 1UL << 1; break;
        case MODE_CROSS:
        default:         inten = 1UL << 3; break;
    }
    reg32(COMP_BASE, COMP_INTENSET) = inten;
    enableNvic(COMP_IRQ);
}

void NrfComp::detachInterrupt() { attachInterrupt(MODE_CROSS, nullptr); }

void NrfComp::serviceIrq() {
    // Clear any fired events; call the user callback once per IRQ entry.
    bool fired = false;
    if (reg32(COMP_BASE, COMP_EVENTS_CROSS) != 0UL) { reg32(COMP_BASE, COMP_EVENTS_CROSS) = 0UL; fired = true; }
    if (reg32(COMP_BASE, COMP_EVENTS_UP)    != 0UL) { reg32(COMP_BASE, COMP_EVENTS_UP)    = 0UL; fired = true; }
    if (reg32(COMP_BASE, COMP_EVENTS_DOWN)  != 0UL) { reg32(COMP_BASE, COMP_EVENTS_DOWN)  = 0UL; fired = true; }
    if (fired && g_compCallback != nullptr) g_compCallback();
}

// ============================================================================
// NrfLpComp - low-power comparator (shares base 0x40013000 + IRQ 19 with COMP)
// ============================================================================

namespace {
constexpr uint32_t LPCOMP_BASE           = 0x40013000UL;
constexpr uint32_t LPCOMP_TASKS_START    = 0x000UL;
constexpr uint32_t LPCOMP_TASKS_STOP     = 0x004UL;
constexpr uint32_t LPCOMP_TASKS_SAMPLE   = 0x008UL;
constexpr uint32_t LPCOMP_EVENTS_READY   = 0x100UL;
constexpr uint32_t LPCOMP_EVENTS_DOWN    = 0x104UL;
constexpr uint32_t LPCOMP_EVENTS_UP      = 0x108UL;
constexpr uint32_t LPCOMP_EVENTS_CROSS   = 0x10CUL;
constexpr uint32_t LPCOMP_INTENSET       = 0x304UL;
constexpr uint32_t LPCOMP_INTENCLR       = 0x308UL;
constexpr uint32_t LPCOMP_RESULT         = 0x400UL;
constexpr uint32_t LPCOMP_ENABLE         = 0x500UL;
constexpr uint32_t LPCOMP_PSEL           = 0x504UL;
constexpr uint32_t LPCOMP_REFSEL         = 0x508UL;
constexpr uint32_t LPCOMP_ANADETECT      = 0x520UL;
constexpr uint32_t LPCOMP_HYST           = 0x538UL;

constexpr uint8_t LPCOMP_IRQ = 19U;   // COMP_LPCOMP_IRQHandler (shared)

nrfLpCompCallback_t g_lpcompCallback = nullptr;
}

bool NrfLpComp::begin(InputPin input, Reference ref, bool hysteresis) {
    reg32(LPCOMP_BASE, LPCOMP_ENABLE) = 0UL;     // takes the block from COMP too
    reg32(LPCOMP_BASE, LPCOMP_PSEL)   = static_cast<uint32_t>(input);
    reg32(LPCOMP_BASE, LPCOMP_REFSEL) = static_cast<uint32_t>(ref);
    reg32(LPCOMP_BASE, LPCOMP_HYST)   = hysteresis ? 1UL : 0UL;
    reg32(LPCOMP_BASE, LPCOMP_ANADETECT) = 0UL;  // default CROSS
    reg32(LPCOMP_BASE, LPCOMP_ENABLE) = 1UL;     // LPCOMP enable value

    reg32(LPCOMP_BASE, LPCOMP_EVENTS_READY) = 0UL;
    reg32(LPCOMP_BASE, LPCOMP_TASKS_START)  = 1UL;
    for (uint32_t spin = 0; spin < 100000UL; ++spin) {
        if (reg32(LPCOMP_BASE, LPCOMP_EVENTS_READY) != 0UL) return true;
    }
    return false;
}

void NrfLpComp::end() {
    reg32(LPCOMP_BASE, LPCOMP_TASKS_STOP) = 1UL;
    reg32(LPCOMP_BASE, LPCOMP_ENABLE)     = 0UL;
    reg32(LPCOMP_BASE, LPCOMP_INTENCLR)   = 0xFFFFFFFFUL;
    disableNvic(LPCOMP_IRQ);
    g_lpcompCallback = nullptr;
}

bool NrfLpComp::isAbove() {
    reg32(LPCOMP_BASE, LPCOMP_TASKS_SAMPLE) = 1UL;
    return (reg32(LPCOMP_BASE, LPCOMP_RESULT) & 1UL) != 0UL;
}

void NrfLpComp::attachInterrupt(Mode mode, nrfLpCompCallback_t cb) {
    g_lpcompCallback = cb;
    reg32(LPCOMP_BASE, LPCOMP_INTENCLR) = 0xFFFFFFFFUL;
    if (cb == nullptr) return;
    // ANADETECT picks which crossing the analog block flags: 0 cross, 1 up,
    // 2 down. Drive the matching event + interrupt.
    reg32(LPCOMP_BASE, LPCOMP_EVENTS_DOWN)  = 0UL;
    reg32(LPCOMP_BASE, LPCOMP_EVENTS_UP)    = 0UL;
    reg32(LPCOMP_BASE, LPCOMP_EVENTS_CROSS) = 0UL;
    uint32_t anadetect = 0UL;
    uint32_t inten = 0UL;
    switch (mode) {
        case MODE_UP:    anadetect = 1UL; inten = 1UL << 2; break;
        case MODE_DOWN:  anadetect = 2UL; inten = 1UL << 1; break;
        case MODE_CROSS:
        default:         anadetect = 0UL; inten = 1UL << 3; break;
    }
    reg32(LPCOMP_BASE, LPCOMP_ANADETECT) = anadetect;
    reg32(LPCOMP_BASE, LPCOMP_INTENSET)  = inten;
    enableNvic(LPCOMP_IRQ);
}

void NrfLpComp::detachInterrupt() { attachInterrupt(MODE_CROSS, nullptr); }

void NrfLpComp::serviceIrq() {
    bool fired = false;
    if (reg32(LPCOMP_BASE, LPCOMP_EVENTS_CROSS) != 0UL) { reg32(LPCOMP_BASE, LPCOMP_EVENTS_CROSS) = 0UL; fired = true; }
    if (reg32(LPCOMP_BASE, LPCOMP_EVENTS_UP)    != 0UL) { reg32(LPCOMP_BASE, LPCOMP_EVENTS_UP)    = 0UL; fired = true; }
    if (reg32(LPCOMP_BASE, LPCOMP_EVENTS_DOWN)  != 0UL) { reg32(LPCOMP_BASE, LPCOMP_EVENTS_DOWN)  = 0UL; fired = true; }
    if (fired && g_lpcompCallback != nullptr) g_lpcompCallback();
}

// COMP and LPCOMP share IRQ 19. They're mutually exclusive, so dispatching to
// both is safe - only the active peripheral has pending events to clear.
extern "C" void COMP_LPCOMP_IRQHandler(void) {
    NrfComp::serviceIrq();
    NrfLpComp::serviceIrq();
}

// ============================================================================
// NrfMwu - memory watch unit
// ============================================================================

namespace {
constexpr uint32_t MWU_BASE = 0x40020000UL;
constexpr uint32_t MWU_INTENSET = 0x304UL;
constexpr uint32_t MWU_INTENCLR = 0x308UL;
constexpr uint32_t MWU_REGIONEN = 0x510UL;
constexpr uint32_t MWU_REGIONENSET = 0x514UL;
constexpr uint32_t MWU_REGIONENCLR = 0x518UL;
constexpr uint32_t MWU_REGION_BASE = 0x600UL;   // REGION[i].START @ +i*16
constexpr uint32_t MWU_REGION_START_OFF = 0x000UL;
constexpr uint32_t MWU_REGION_END_OFF   = 0x004UL;
constexpr uint32_t MWU_PEREGION_RA      = 0x668UL;   // PERREGION[i].EVENTS_RAMATCH base
constexpr uint32_t MWU_PEREGION_WA      = 0x66CUL;   // PERREGION[i].EVENTS_WAMATCH base
constexpr uint32_t MWU_PEREGION_STRIDE  = 0x024UL;

constexpr uint8_t MWU_IRQ = 32U;   // MWU_IRQn

nrfMwuCallback_t g_mwuCallback = nullptr;
}

bool NrfMwu::watchRegion(uint8_t region, uint32_t startAddr, uint32_t endAddr,
                          bool catchWrites, bool catchReads) {
    if (region >= REGION_COUNT) return false;
    if (endAddr <= startAddr) return false;

    // Set region bounds.
    *reinterpret_cast<volatile uint32_t *>(MWU_BASE + MWU_REGION_BASE + (region * 16U)) = startAddr;
    *reinterpret_cast<volatile uint32_t *>(MWU_BASE + MWU_REGION_BASE + (region * 16U) + 4U) = endAddr;

    // Enable read/write detection on this region.
    uint32_t enableMask = 0UL;
    if (catchReads)  enableMask |= (1UL << (region * 2U));        // RA bit
    if (catchWrites) enableMask |= (1UL << (region * 2U + 1U));   // WA bit
    reg32(MWU_BASE, MWU_REGIONENSET) = enableMask;
    return true;
}

void NrfMwu::unwatchRegion(uint8_t region) {
    if (region >= REGION_COUNT) return;
    reg32(MWU_BASE, MWU_REGIONENCLR) = (3UL << (region * 2U));
}

void NrfMwu::attachInterrupt(nrfMwuCallback_t cb) {
    g_mwuCallback = cb;
    if (cb == nullptr) {
        reg32(MWU_BASE, MWU_INTENCLR) = 0xFFFFFFFFUL;
        return;
    }
    // Enable RAMATCH + WAMATCH IRQ for all 4 regions.
    uint32_t inten = 0UL;
    for (uint8_t i = 0; i < REGION_COUNT; ++i) {
        inten |= (1UL << (i * 2U));        // RAMATCH[i]
        inten |= (1UL << (i * 2U + 1U));   // WAMATCH[i]
    }
    reg32(MWU_BASE, MWU_INTENSET) = inten;
    enableNvic(MWU_IRQ);
}

void NrfMwu::detachInterrupt() { attachInterrupt(nullptr); }

void NrfMwu::serviceIrq() {
    if (g_mwuCallback == nullptr) return;
    // For each region, check both event types and clear.
    for (uint8_t i = 0; i < REGION_COUNT; ++i) {
        const uint32_t raOff = MWU_PEREGION_RA + (i * MWU_PEREGION_STRIDE);
        const uint32_t waOff = MWU_PEREGION_WA + (i * MWU_PEREGION_STRIDE);
        if (reg32(MWU_BASE, raOff) != 0UL) {
            reg32(MWU_BASE, raOff) = 0UL;
            g_mwuCallback(/*addr*/ 0UL, /*write*/ false);
        }
        if (reg32(MWU_BASE, waOff) != 0UL) {
            reg32(MWU_BASE, waOff) = 0UL;
            g_mwuCallback(/*addr*/ 0UL, /*write*/ true);
        }
    }
}

extern "C" void MWU_IRQHandler(void) { NrfMwu::serviceIrq(); }

// ============================================================================
// NrfGpioteOut - GPIOTE output channel for PPI use
// ============================================================================

namespace {
constexpr uint32_t GPIOTE_BASE = 0x40006000UL;
constexpr uint32_t GPIOTE_TASKS_OUT_BASE = 0x000UL;   // CH[i] @ +i*4
constexpr uint32_t GPIOTE_TASKS_SET_BASE = 0x030UL;
constexpr uint32_t GPIOTE_TASKS_CLR_BASE = 0x060UL;
constexpr uint32_t GPIOTE_CONFIG_BASE    = 0x510UL;   // CH[i].CONFIG @ +i*4

uint8_t g_gpioteOutAllocated = 0;   // bit i = CH[i] in use as output

// CONFIG layout:
//   bits 0..1   MODE (0=disabled, 1=event input, 3=task output)
//   bits 8..12  PSEL pin number
//   bit  13     PORT (0=P0, 1=P1)
//   bits 16..17 POLARITY (0=none, 1=low->high, 2=high->low, 3=toggle)
//   bit  20     OUTINIT (initial output value when mode=3)

uint32_t gpioteConfigForOutput(uint8_t pin, bool initialHigh, uint8_t taskMode) {
    const uint32_t pinIdx = pin & 0x1FU;
    const uint32_t port = (pin >> 5U) & 1U;
    uint32_t cfg = 3UL;                 // MODE = task output
    cfg |= (pinIdx << 8);               // PSEL
    cfg |= (port << 13);                // PORT
    cfg |= (static_cast<uint32_t>(taskMode) << 16);   // POLARITY
    if (initialHigh) cfg |= (1UL << 20);
    return cfg;
}
}

uint8_t NrfGpioteOut::allocate(uint8_t pin, bool initialHigh, TaskMode taskMode) {
    for (uint8_t ch = 0; ch < 8U; ++ch) {
        if ((g_gpioteOutAllocated & (1U << ch)) != 0U) continue;
        g_gpioteOutAllocated |= (1U << ch);
        reg32(GPIOTE_BASE, GPIOTE_CONFIG_BASE + (ch * 4U)) =
            gpioteConfigForOutput(pin, initialHigh, static_cast<uint8_t>(taskMode));
        return ch;
    }
    return INVALID_CHANNEL;
}

void NrfGpioteOut::release(uint8_t channel) {
    if (channel >= 8U) return;
    reg32(GPIOTE_BASE, GPIOTE_CONFIG_BASE + (channel * 4U)) = 0UL;
    g_gpioteOutAllocated &= ~(1U << channel);
}

void NrfGpioteOut::triggerOut(uint8_t channel) {
    if (channel >= 8U) return;
    reg32(GPIOTE_BASE, GPIOTE_TASKS_OUT_BASE + (channel * 4U)) = 1UL;
}

uint32_t NrfGpioteOut::taskOutAddr(uint8_t channel) {
    if (channel >= 8U) return 0UL;
    return GPIOTE_BASE + GPIOTE_TASKS_OUT_BASE + (channel * 4U);
}
uint32_t NrfGpioteOut::taskSetAddr(uint8_t channel) {
    if (channel >= 8U) return 0UL;
    return GPIOTE_BASE + GPIOTE_TASKS_SET_BASE + (channel * 4U);
}
uint32_t NrfGpioteOut::taskClrAddr(uint8_t channel) {
    if (channel >= 8U) return 0UL;
    return GPIOTE_BASE + GPIOTE_TASKS_CLR_BASE + (channel * 4U);
}
