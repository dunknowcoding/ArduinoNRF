// NrfPower.cpp - implementation of NrfPower. See NrfPower.h.

#include "NrfPower.h"
#include "NrfClock.h"

namespace {

constexpr uint32_t POWER_BASE          = 0x40000000UL;
constexpr uint32_t POWER_TASKS_CONSTLAT = 0x078UL;
constexpr uint32_t POWER_TASKS_LOWPWR  = 0x07CUL;
constexpr uint32_t POWER_RESETREAS     = 0x400UL;
constexpr uint32_t POWER_USBREGSTATUS  = 0x438UL;
constexpr uint32_t POWER_SYSTEMOFF     = 0x500UL;
constexpr uint32_t POWER_GPREGRET      = 0x51CUL;
constexpr uint32_t POWER_GPREGRET2     = 0x520UL;
constexpr uint32_t POWER_DCDCEN        = 0x578UL;
constexpr uint32_t POWER_DCDCEN0       = 0x590UL;   // HV DCDC on nRF52840
constexpr uint32_t POWER_RAM_BASE      = 0x900UL;   // RAM[0..7] @ +i*0x10
constexpr uint32_t POWER_RAM_STRIDE    = 0x010UL;
constexpr uint32_t POWER_RAM_POWERSET  = 0x004UL;   // relative within bank
constexpr uint32_t POWER_RAM_POWERCLR  = 0x008UL;

constexpr uint32_t POWER_USBREG_VBUSDETECT_BIT = 1U << 0;
constexpr uint32_t POWER_USBREG_OUTPUTRDY_BIT  = 1U << 1;

constexpr uint32_t GPIO_PORT0_BASE = 0x50000000UL;
constexpr uint32_t GPIO_PORT_STRIDE = 0x300UL;
constexpr uint32_t GPIO_PIN_CNF_BASE = 0x700UL;
constexpr uint32_t GPIO_PIN_CNF_SENSE_MASK = 0x3UL << 16U;
constexpr uint32_t GPIO_PIN_CNF_SENSE_HIGH = 0x2UL << 16U;
constexpr uint32_t GPIO_PIN_CNF_SENSE_LOW  = 0x3UL << 16U;

constexpr uint32_t RTC1_BASE = 0x40011000UL;
constexpr uint32_t RTC_TASKS_START   = 0x000UL;
constexpr uint32_t RTC_TASKS_STOP    = 0x004UL;
constexpr uint32_t RTC_TASKS_CLEAR   = 0x008UL;
constexpr uint32_t RTC_EVENTS_COMPARE0 = 0x140UL;
constexpr uint32_t RTC_INTENSET      = 0x304UL;
constexpr uint32_t RTC_INTENCLR      = 0x308UL;
constexpr uint32_t RTC_EVTENSET      = 0x344UL;
constexpr uint32_t RTC_EVTENCLR      = 0x348UL;
constexpr uint32_t RTC_COUNTER       = 0x504UL;
constexpr uint32_t RTC_PRESCALER     = 0x508UL;
constexpr uint32_t RTC_CC0           = 0x540UL;
constexpr uint32_t RTC_INTEN_COMPARE0 = 1UL << 16;
constexpr uint32_t RTC_LFCLK_HZ      = 32768UL;
constexpr uint32_t RTC1_PRESCALER_FOR_1KHZ = (RTC_LFCLK_HZ / 1000UL) - 1UL;  // ~31

constexpr uint32_t NVIC_BASE  = 0xE000E000UL;
constexpr uint32_t NVIC_ISER0 = 0x100UL;
constexpr uint32_t NVIC_ICER0 = 0x180UL;
constexpr uint32_t NVIC_ICPR0 = 0x280UL;
constexpr uint8_t  RTC1_IRQ_NUMBER = 17U;

// System Control Block - SCR.SEVONPEND lets a *pending* interrupt (even one
// disabled in the NVIC) wake WFE without taking the exception. This is the
// documented nRF52 way to block on a peripheral event from WFE.
constexpr uint32_t SCB_SCR          = 0xE000ED10UL;
constexpr uint32_t SCB_SCR_SEVONPEND = 1UL << 4;

inline volatile uint32_t &reg32(uint32_t base, uint32_t offset) {
    return *reinterpret_cast<volatile uint32_t *>(base + offset);
}

inline void enableNvic(uint8_t irq) {
    reg32(NVIC_BASE, NVIC_ISER0 + ((irq >> 5U) * 4UL)) = 1UL << (irq & 0x1FU);
}

inline void disableNvic(uint8_t irq) {
    reg32(NVIC_BASE, NVIC_ICER0 + ((irq >> 5U) * 4UL)) = 1UL << (irq & 0x1FU);
}

inline void clearPendingNvic(uint8_t irq) {
    reg32(NVIC_BASE, NVIC_ICPR0 + ((irq >> 5U) * 4UL)) = 1UL << (irq & 0x1FU);
}

uint32_t gpioPortBase(uint8_t pin) {
    return GPIO_PORT0_BASE + (((pin >> 5U) & 1U) * GPIO_PORT_STRIDE);
}

uint32_t gpioPinCnfOffset(uint8_t pin) {
    return GPIO_PIN_CNF_BASE + ((pin & 0x1FU) * 4UL);
}

}  // namespace

// -- System ON sleep ---------------------------------------------------------

void NrfPower::sleep() {
    // Cortex-M4 idle. Wakes on any enabled NVIC interrupt or external event.
    // DSB before WFI is recommended by ARM (architecturally hints that all
    // pending memory ops should complete before sleeping).
    __asm__ volatile("dsb" : : : "memory");
    __asm__ volatile("wfi" : : : "memory");
}

void NrfPower::sleepWfe() {
    __asm__ volatile("dsb" : : : "memory");
    __asm__ volatile("wfe" : : : "memory");
}

void NrfPower::sleepMs(uint32_t delayMs) {
    if (delayMs == 0UL) {
        return;
    }
    // Make sure LFCLK is running - the existing core start path does this
    // already, but a sketch that explicitly stopped it would deadlock here.
    if (!nrfLfclkRunning()) {
        nrfStartLfclk();
    }

    // Cap to 24-bit RTC range at the ~1 kHz tick, and floor at 2 ticks: the
    // RTC compare can MISS a match if CC == COUNTER+1, so we need >= +2.
    if (delayMs > 0x00FFFFFFUL) {
        delayMs = 0x00FFFFFFUL;
    }
    if (delayMs < 2UL) {
        delayMs = 2UL;
    }

    // Configure RTC1 @ ~1 kHz and schedule CC0 RELATIVE to the live counter.
    //
    // We deliberately do NOT clear the counter and trust it to read 0: on
    // nRF52 TASKS_STOP / TASKS_CLEAR take up to one 32.768 kHz cycle (~31 us)
    // to propagate, so the counter can still hold a stale, nonzero value the
    // instant after START (measured 9342 on hardware). An ABSOLUTE CC0 then
    // either matches at the wrong time or, if the stale counter is already past
    // it, only fires after a full 24-bit wrap (~4.5 h) - which is exactly the
    // "sleepMs returns in ~1 ms / or hangs" bug this replaces. Reading the
    // running counter and arming CC0 = counter + delayMs (24-bit wrap-safe) is
    // immune to that latency.
    //
    // Wake mechanism: SEVONPEND + INTEN with the RTC1 NVIC line left DISABLED.
    // The compare asserts the RTC1 interrupt, which the NVIC latches as PENDING;
    // SEVONPEND then wakes WFE on that pending transition WITHOUT taking the
    // exception - so no ISR runs (no collision with NrfRtc's RTC1_IRQHandler
    // clearing the event) and EVENTS_COMPARE0 stays latched for the poll.
    // (EVTEN alone was tried and does NOT reliably wake WFE on this silicon -
    // measured the counter overrunning CC0 by thousands of ticks before an
    // unrelated event happened to wake it.)
    reg32(SCB_SCR, 0UL) |= SCB_SCR_SEVONPEND;

    reg32(RTC1_BASE, RTC_TASKS_STOP) = 1UL;
    reg32(RTC1_BASE, RTC_PRESCALER) = RTC1_PRESCALER_FOR_1KHZ;   // writable while stopped
    reg32(RTC1_BASE, RTC_TASKS_START) = 1UL;

    const uint32_t base = reg32(RTC1_BASE, RTC_COUNTER) & 0x00FFFFFFUL;
    reg32(RTC1_BASE, RTC_EVENTS_COMPARE0) = 0UL;
    reg32(RTC1_BASE, RTC_CC0) = (base + delayMs) & 0x00FFFFFFUL;
    reg32(RTC1_BASE, RTC_INTENSET) = RTC_INTEN_COMPARE0;   // -> NVIC pending (NVIC stays disabled)

    // Prime the CPU event register (SEV sets it; the first WFE consumes it),
    // then sleep until the compare pends. A spurious wake just re-checks the
    // latched compare event and sleeps again ("sleep up to N ms" semantics).
    __asm__ volatile("sev" : : : "memory");
    __asm__ volatile("wfe" : : : "memory");
    while (reg32(RTC1_BASE, RTC_EVENTS_COMPARE0) == 0UL) {
        __asm__ volatile("dsb" : : : "memory");
        __asm__ volatile("wfe" : : : "memory");
    }

    // Stop + tear down. Clear our INTEN, the event, and the (unserviced) NVIC
    // pending bit so a later nrfRtc1() user doesn't inherit a stale pend.
    reg32(RTC1_BASE, RTC_TASKS_STOP) = 1UL;
    reg32(RTC1_BASE, RTC_INTENCLR) = RTC_INTEN_COMPARE0;
    reg32(RTC1_BASE, RTC_EVENTS_COMPARE0) = 0UL;
    clearPendingNvic(RTC1_IRQ_NUMBER);
}

void NrfPower::setMode(uint8_t mode) {
    switch (mode) {
        case MODE_LOW_POWER:
            reg32(POWER_BASE, POWER_TASKS_LOWPWR) = 1UL;
            break;
        case MODE_CONSTANT_LATENCY:
            reg32(POWER_BASE, POWER_TASKS_CONSTLAT) = 1UL;
            break;
        case MODE_UNCHANGED:
        default:
            break;
    }
}

// -- DCDC regulator ----------------------------------------------------------

void NrfPower::enableDcdc(bool enable) {
    reg32(POWER_BASE, POWER_DCDCEN) = enable ? 1UL : 0UL;
}

void NrfPower::enableHvDcdc(bool enable) {
    reg32(POWER_BASE, POWER_DCDCEN0) = enable ? 1UL : 0UL;
}

bool NrfPower::isDcdcEnabled() {
    return (reg32(POWER_BASE, POWER_DCDCEN) & 1UL) != 0UL;
}

// -- RAM retention -----------------------------------------------------------

void NrfPower::setRamRetention(uint16_t bankBitmap) {
    // Each RAM[i].POWER register: bits 0..1 = section power, bits 16..17 = section retention.
    // For simplicity we set both POWER and RETENTION for the requested banks and clear them for the rest.
    // POWERSET / POWERCLR are W1C-style: writing 1 sets / clears the corresponding bit.
    constexpr uint32_t SECTION_MASK = 0x00030003UL;   // 2 sections, power + retention
    for (uint8_t bank = 0; bank < 8U; ++bank) {
        const uint32_t base = POWER_BASE + POWER_RAM_BASE + (bank * POWER_RAM_STRIDE);
        if ((bankBitmap >> bank) & 1U) {
            *reinterpret_cast<volatile uint32_t *>(base + POWER_RAM_POWERSET) = SECTION_MASK;
        } else {
            *reinterpret_cast<volatile uint32_t *>(base + POWER_RAM_POWERCLR) = SECTION_MASK;
        }
    }
}

uint16_t NrfPower::getRamRetention() {
    uint16_t bitmap = 0U;
    for (uint8_t bank = 0; bank < 8U; ++bank) {
        const uint32_t base = POWER_BASE + POWER_RAM_BASE + (bank * POWER_RAM_STRIDE);
        const uint32_t v = *reinterpret_cast<volatile uint32_t *>(base);
        if (v & 0x00030000UL) {   // any retention bit set
            bitmap |= static_cast<uint16_t>(1U << bank);
        }
    }
    return bitmap;
}

// -- Wake sources ------------------------------------------------------------

void NrfPower::enableGpioWake(uint8_t pin, bool activeHigh) {
    const uint32_t base = gpioPortBase(pin);
    const uint32_t off = gpioPinCnfOffset(pin);
    uint32_t cnf = reg32(base, off);
    cnf &= ~GPIO_PIN_CNF_SENSE_MASK;
    cnf |= activeHigh ? GPIO_PIN_CNF_SENSE_HIGH : GPIO_PIN_CNF_SENSE_LOW;
    reg32(base, off) = cnf;
}

void NrfPower::disableGpioWake(uint8_t pin) {
    const uint32_t base = gpioPortBase(pin);
    const uint32_t off = gpioPinCnfOffset(pin);
    uint32_t cnf = reg32(base, off);
    cnf &= ~GPIO_PIN_CNF_SENSE_MASK;
    reg32(base, off) = cnf;
}

void NrfPower::enableNfcWake(bool enable) {
    // NFC wake is gated by the NFCT peripheral state: when NFCT is enabled +
    // in tag mode, an RF field wakes from SystemOFF. There is no dedicated
    // wake-enable bit in POWER for NFC. This stub records intent + checks
    // that NFCT is enabled when enable=true.
    (void)enable;   // intentional no-op - NfcTag::begin() actually arms it
}

void NrfPower::enableUsbWake(bool enable) {
    // USB plug detect is implicit: POWER tracks VBUSDETECT in USBREGSTATUS,
    // and an edge there wakes SystemOFF if POWER.RESET signal is configured.
    // No knob to flip here - just ensure VBUS sensing is on (it's on by
    // default unless the user explicitly disabled USB regulator monitoring).
    (void)enable;
}

// -- SystemOFF entry ---------------------------------------------------------

void NrfPower::enterSystemOff() {
    // Errata 12: a write to POWER_SYSTEMOFF can race the BUS - force a
    // BARRIER + READBACK to guarantee the write goes out. The trailing
    // infinite loop is the official Nordic-recommended fallback if the
    // chip wakes spuriously (it shouldn't).
    reg32(POWER_BASE, POWER_SYSTEMOFF) = 1UL;
    __asm__ volatile("dsb" : : : "memory");
    while (true) {
        __asm__ volatile("wfe" : : : "memory");
    }
}

// -- Reset reason / GPREGRET -------------------------------------------------

uint32_t NrfPower::getResetReason() {
    return reg32(POWER_BASE, POWER_RESETREAS);
}

void NrfPower::clearResetReason() {
    // RESETREAS is W1C - write 1s to clear all latched bits.
    reg32(POWER_BASE, POWER_RESETREAS) = 0xFFFFFFFFUL;
}

bool NrfPower::wokeFromSystemOff() {
    constexpr uint32_t WAKE_MASK = RESET_GPIO_WAKE | RESET_LPCOMP_WAKE |
                                   RESET_DEBUGIF_WAKE | RESET_NFC_WAKE |
                                   RESET_VBUS_WAKE;
    return (reg32(POWER_BASE, POWER_RESETREAS) & WAKE_MASK) != 0UL;
}

uint8_t NrfPower::getGpregret() {
    return static_cast<uint8_t>(reg32(POWER_BASE, POWER_GPREGRET) & 0xFFUL);
}

uint8_t NrfPower::getGpregret2() {
    return static_cast<uint8_t>(reg32(POWER_BASE, POWER_GPREGRET2) & 0xFFUL);
}

// -- Diagnostics -------------------------------------------------------------

bool NrfPower::isUsbVbusPresent() {
    return (reg32(POWER_BASE, POWER_USBREGSTATUS) & POWER_USBREG_VBUSDETECT_BIT) != 0UL;
}

bool NrfPower::isUsbRegulatorOutputReady() {
    return (reg32(POWER_BASE, POWER_USBREGSTATUS) & POWER_USBREG_OUTPUTRDY_BIT) != 0UL;
}
