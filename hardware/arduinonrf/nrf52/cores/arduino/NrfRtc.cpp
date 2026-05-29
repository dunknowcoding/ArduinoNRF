// NrfRtc.cpp - implementation of the nRF52 RTC0/1/2 driver. See NrfRtc.h.

#include "NrfRtc.h"
#include "NrfClock.h"

namespace {

constexpr uint32_t RTC0_BASE = 0x4000B000UL;
constexpr uint32_t RTC1_BASE = 0x40011000UL;
constexpr uint32_t RTC2_BASE = 0x40024000UL;

// Register offsets, identical across RTC0..RTC2.
constexpr uint32_t RTC_TASKS_START    = 0x000UL;
constexpr uint32_t RTC_TASKS_STOP     = 0x004UL;
constexpr uint32_t RTC_TASKS_CLEAR    = 0x008UL;
constexpr uint32_t RTC_EVENTS_TICK    = 0x100UL;
constexpr uint32_t RTC_EVENTS_OVRFLW  = 0x104UL;
constexpr uint32_t RTC_EVENTS_COMPARE = 0x140UL;   // CC0..CC3 at +0, +4, +8, +C
constexpr uint32_t RTC_INTENSET       = 0x304UL;
constexpr uint32_t RTC_INTENCLR       = 0x308UL;
constexpr uint32_t RTC_EVTENSET       = 0x344UL;
constexpr uint32_t RTC_EVTENCLR       = 0x348UL;
constexpr uint32_t RTC_COUNTER        = 0x504UL;
constexpr uint32_t RTC_PRESCALER      = 0x508UL;
constexpr uint32_t RTC_CC0            = 0x540UL;   // CC0..CC3 at +0, +4, +8, +C

constexpr uint32_t RTC_INTEN_TICK     = 1UL << 0;
constexpr uint32_t RTC_INTEN_OVRFLW   = 1UL << 1;
constexpr uint32_t RTC_INTEN_COMPARE0 = 1UL << 16;
constexpr uint32_t RTC_INTEN_COMPARE_BASE_BIT = 16U;

constexpr uint32_t RTC_LFCLK_HZ = 32768UL;
constexpr uint32_t RTC_PRESCALER_MAX = 4095UL;     // 12-bit field

constexpr uint32_t NVIC_BASE  = 0xE000E000UL;
constexpr uint32_t NVIC_ISER0 = 0x100UL;
constexpr uint32_t NVIC_ICER0 = 0x180UL;

// nRF52840 IRQ numbers (matches Nordic's nrf52840.h):
//   RTC0 = 11, RTC1 = 17, RTC2 = 36.
constexpr uint8_t RTC0_IRQ_NUMBER = 11U;
constexpr uint8_t RTC1_IRQ_NUMBER = 17U;
constexpr uint8_t RTC2_IRQ_NUMBER = 36U;

inline volatile uint32_t &reg32(uint32_t base, uint32_t offset) {
    return *reinterpret_cast<volatile uint32_t *>(base + offset);
}

void enableNvic(uint8_t irqNumber) {
    if (irqNumber >= 64U) return;
    const uint32_t bit = 1UL << (irqNumber & 0x1FU);
    reg32(NVIC_BASE, NVIC_ISER0 + ((irqNumber >> 5U) * 4UL)) = bit;
}

void disableNvic(uint8_t irqNumber) {
    if (irqNumber >= 64U) return;
    const uint32_t bit = 1UL << (irqNumber & 0x1FU);
    reg32(NVIC_BASE, NVIC_ICER0 + ((irqNumber >> 5U) * 4UL)) = bit;
}

uint32_t baseForIndex(uint8_t index) {
    switch (index) {
        case 0U: return RTC0_BASE;
        case 1U: return RTC1_BASE;
        case 2U: return RTC2_BASE;
        default: return 0U;
    }
}

uint8_t irqForIndex(uint8_t index) {
    switch (index) {
        case 0U: return RTC0_IRQ_NUMBER;
        case 1U: return RTC1_IRQ_NUMBER;
        case 2U: return RTC2_IRQ_NUMBER;
        default: return 0xFFU;
    }
}

// Choose a prescaler so the resulting tick rate is as close to tickHz as the
// 12-bit divider allows. Returns 0xFFFFFFFF on invalid request.
uint32_t prescalerForTickHz(uint32_t tickHz) {
    if (tickHz == 0UL || tickHz > RTC_LFCLK_HZ) return 0xFFFFFFFFUL;
    // freq = LFCLK / (1 + prescaler) -> prescaler = LFCLK / freq - 1.
    uint32_t prescaler = (RTC_LFCLK_HZ + (tickHz / 2UL)) / tickHz;
    if (prescaler > 0UL) prescaler -= 1UL;
    if (prescaler > RTC_PRESCALER_MAX) prescaler = RTC_PRESCALER_MAX;
    return prescaler;
}

}  // namespace

// ---- NrfRtc -----------------------------------------------------------------

NrfRtc::NrfRtc(uint8_t index)
    : base_(baseForIndex(index)),
      index_(index),
      irqNumber_(irqForIndex(index)),
      tickHz_(0UL),
      overflowCallback_(nullptr) {
    for (uint8_t i = 0; i < COMPARE_CHANNEL_COUNT; ++i) {
        compareCallbacks_[i] = nullptr;
    }
}

bool NrfRtc::begin(uint32_t tickHz) {
    if (!isValid()) return false;
    const uint32_t prescaler = prescalerForTickHz(tickHz);
    if (prescaler == 0xFFFFFFFFUL) return false;

    if (!nrfLfclkRunning()) {
        nrfStartLfclk();
    }

    // Configure stopped before touching PRESCALER (datasheet requirement).
    reg32(base_, RTC_TASKS_STOP) = 1UL;
    reg32(base_, RTC_TASKS_CLEAR) = 1UL;
    reg32(base_, RTC_PRESCALER) = prescaler;
    // Clear any pending events from a previous session.
    reg32(base_, RTC_EVENTS_TICK) = 0UL;
    reg32(base_, RTC_EVENTS_OVRFLW) = 0UL;
    for (uint8_t i = 0; i < COMPARE_CHANNEL_COUNT; ++i) {
        reg32(base_, RTC_EVENTS_COMPARE + (i * 4U)) = 0UL;
    }
    // Disable all interrupts; user re-enables via attachXInterrupt.
    reg32(base_, RTC_INTENCLR) = 0xFFFFFFFFUL;
    reg32(base_, RTC_EVTENCLR) = 0xFFFFFFFFUL;

    // Recompute the actual tick rate (rounded to the nearest divider).
    tickHz_ = RTC_LFCLK_HZ / (prescaler + 1UL);
    return true;
}

void NrfRtc::end() {
    if (!isValid()) return;
    reg32(base_, RTC_TASKS_STOP) = 1UL;
    reg32(base_, RTC_INTENCLR) = 0xFFFFFFFFUL;
    reg32(base_, RTC_EVTENCLR) = 0xFFFFFFFFUL;
    disableNvic(irqNumber_);
    overflowCallback_ = nullptr;
    for (uint8_t i = 0; i < COMPARE_CHANNEL_COUNT; ++i) {
        compareCallbacks_[i] = nullptr;
    }
    tickHz_ = 0UL;
}

void NrfRtc::start() {
    if (!isValid()) return;
    reg32(base_, RTC_TASKS_START) = 1UL;
}

void NrfRtc::stop() {
    if (!isValid()) return;
    reg32(base_, RTC_TASKS_STOP) = 1UL;
}

void NrfRtc::clear() {
    if (!isValid()) return;
    reg32(base_, RTC_TASKS_CLEAR) = 1UL;
}

bool NrfRtc::isRunning() const {
    // The RTC has no public "running" status bit; track it via tickHz_ being
    // set after begin() with no end() since then. Best-effort.
    return isValid() && tickHz_ != 0UL;
}

uint32_t NrfRtc::counter() const {
    if (!isValid()) return 0UL;
    return reg32(base_, RTC_COUNTER) & 0x00FFFFFFUL;
}

uint32_t NrfRtc::prescaler() const {
    if (!isValid()) return 0UL;
    return reg32(base_, RTC_PRESCALER) & 0x00000FFFUL;
}

uint32_t NrfRtc::periodUs() const {
    if (tickHz_ == 0UL) return 0UL;
    return (1000000UL + (tickHz_ / 2UL)) / tickHz_;
}

void NrfRtc::setCompare(uint8_t ccIndex, uint32_t ticks) {
    if (!isValid() || ccIndex >= COMPARE_CHANNEL_COUNT) return;
    reg32(base_, RTC_CC0 + (ccIndex * 4U)) = ticks & 0x00FFFFFFUL;
}

uint32_t NrfRtc::getCompare(uint8_t ccIndex) const {
    if (!isValid() || ccIndex >= COMPARE_CHANNEL_COUNT) return 0UL;
    return reg32(base_, RTC_CC0 + (ccIndex * 4U)) & 0x00FFFFFFUL;
}

void NrfRtc::attachCompareInterrupt(uint8_t ccIndex, nrfRtcCallback_t callback) {
    if (!isValid() || ccIndex >= COMPARE_CHANNEL_COUNT) return;
    compareCallbacks_[ccIndex] = callback;
    if (callback != nullptr) {
        // Clear stale event, enable EVTEN + INTEN for this compare channel.
        reg32(base_, RTC_EVENTS_COMPARE + (ccIndex * 4U)) = 0UL;
        const uint32_t mask = 1UL << (RTC_INTEN_COMPARE_BASE_BIT + ccIndex);
        reg32(base_, RTC_EVTENSET) = mask;
        reg32(base_, RTC_INTENSET) = mask;
        enableNvic(irqNumber_);
    } else {
        const uint32_t mask = 1UL << (RTC_INTEN_COMPARE_BASE_BIT + ccIndex);
        reg32(base_, RTC_INTENCLR) = mask;
        reg32(base_, RTC_EVTENCLR) = mask;
    }
}

void NrfRtc::detachCompareInterrupt(uint8_t ccIndex) {
    attachCompareInterrupt(ccIndex, nullptr);
}

void NrfRtc::attachOverflowInterrupt(nrfRtcCallback_t callback) {
    if (!isValid()) return;
    overflowCallback_ = callback;
    if (callback != nullptr) {
        reg32(base_, RTC_EVENTS_OVRFLW) = 0UL;
        reg32(base_, RTC_EVTENSET) = RTC_INTEN_OVRFLW;
        reg32(base_, RTC_INTENSET) = RTC_INTEN_OVRFLW;
        enableNvic(irqNumber_);
    } else {
        reg32(base_, RTC_INTENCLR) = RTC_INTEN_OVRFLW;
        reg32(base_, RTC_EVTENCLR) = RTC_INTEN_OVRFLW;
    }
}

void NrfRtc::detachOverflowInterrupt() {
    attachOverflowInterrupt(nullptr);
}

uint32_t NrfRtc::scheduleInMs(uint32_t delayMs) {
    if (!isValid() || tickHz_ == 0UL) return 0UL;
    uint64_t ticksToAdd = (static_cast<uint64_t>(delayMs) * tickHz_ + 500ULL) / 1000ULL;
    if (ticksToAdd > 0x00FFFFFFULL) {
        ticksToAdd = 0x00FFFFFFULL;
    }
    const uint32_t target = (counter() + static_cast<uint32_t>(ticksToAdd)) & 0x00FFFFFFUL;
    setCompare(0U, target);
    return target;
}

void NrfRtc::serviceIrq() {
    if (!isValid()) return;
    if (reg32(base_, RTC_EVENTS_OVRFLW) != 0UL) {
        reg32(base_, RTC_EVENTS_OVRFLW) = 0UL;
        if (overflowCallback_ != nullptr) overflowCallback_();
    }
    for (uint8_t i = 0; i < COMPARE_CHANNEL_COUNT; ++i) {
        const uint32_t off = RTC_EVENTS_COMPARE + (i * 4U);
        if (reg32(base_, off) != 0UL) {
            reg32(base_, off) = 0UL;
            if (compareCallbacks_[i] != nullptr) compareCallbacks_[i]();
        }
    }
}

// ---- Singletons + IRQ glue --------------------------------------------------

NrfRtc &nrfRtc0() { static NrfRtc instance(0U); return instance; }
NrfRtc &nrfRtc1() { static NrfRtc instance(1U); return instance; }
NrfRtc &nrfRtc2() { static NrfRtc instance(2U); return instance; }

extern "C" {

// The vector table calls these for the three RTC NVIC slots. They dispatch to
// the matching singleton, which is the one all user code attaches handlers
// to. startup_nrf52.cpp declares weak aliases pointing at Default_Handler
// for these symbols; our non-weak definitions here override them so the IRQ
// actually reaches user callbacks.
// RTC0 handler is WEAK: when the NimBLE library is linked it owns RTC0 as the
// BLE controller's os_cputime/hal_timer cputimer (its strong RTC0_IRQHandler in
// npl_os_bare.c overrides this). Non-BLE sketches keep this definition for
// nrfRtc0(). RTC1/RTC2 stay strong (the core's sleepMs etc. use them).
__attribute__((weak)) void RTC0_IRQHandler(void) { nrfRtc0().serviceIrq(); }
void RTC1_IRQHandler(void) { nrfRtc1().serviceIrq(); }
void RTC2_IRQHandler(void) { nrfRtc2().serviceIrq(); }

}
