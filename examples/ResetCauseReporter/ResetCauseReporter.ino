#include <stdint.h>

namespace {
constexpr uint32_t DIAG_CAUSE_ADDR = 0x20004004UL;
constexpr uint32_t POWER_GPREGRET = 0x4000051CUL;
constexpr uint32_t SCB_AIRCR = 0xE000ED0CUL;
constexpr uint32_t SYST_CSR = 0xE000E010UL;
constexpr uint32_t SYST_RVR = 0xE000E014UL;
constexpr uint32_t SYST_CVR = 0xE000E018UL;
constexpr uint32_t SYSRESETREQ = 0x05FA0004UL;
constexpr uint32_t DFU_MAGIC_SERIAL_ONLY_RESET = 0x4EUL;
constexpr uint32_t SYST_ENABLE_CLKSOURCE = 0x00000005UL;
constexpr uint32_t SYST_COUNTFLAG = 0x00010000UL;
constexpr uint32_t CPU_CLOCK_HZ = 64000000UL;

constexpr uint32_t CAUSE_CONFIG_TIMEOUT = 0xCA5E0001UL;
constexpr uint32_t CAUSE_POLL_DETACH = 0xCA5E0002UL;
constexpr uint32_t CAUSE_IRQ_DETACH = 0xCA5E0003UL;
constexpr uint32_t CAUSE_1200_TOUCH = 0xCA5E1200UL;
constexpr uint32_t CAUSE_DFU_DETACH = 0xCA5E0DF0UL;
constexpr uint32_t CAUSE_DEFAULT_HANDLER = 0xCA5E0DEFUL;
constexpr uint32_t CAUSE_HARDFAULT = 0xCA5E0FA1UL;
constexpr uint32_t CAUSE_WDT_IRQ = 0xCA5E0AD0UL;
constexpr uint32_t CAUSE_DIAG_STAGE_BASE = 0xCA5ED000UL;

inline volatile uint32_t &reg32(uint32_t address) {
    return *reinterpret_cast<volatile uint32_t *>(address);
}

uint32_t encodedDelaySeconds(uint32_t cause) {
    if ((cause & 0xFFFFFF00UL) == CAUSE_DIAG_STAGE_BASE) {
        return 20UL + (cause & 0xFFUL);
    }

    switch (cause) {
        case CAUSE_CONFIG_TIMEOUT:
            return 4UL;
        case CAUSE_POLL_DETACH:
            return 6UL;
        case CAUSE_IRQ_DETACH:
            return 8UL;
        case CAUSE_1200_TOUCH:
            return 10UL;
        case CAUSE_DFU_DETACH:
            return 12UL;
        case CAUSE_DEFAULT_HANDLER:
            return 14UL;
        case CAUSE_HARDFAULT:
            return 16UL;
        case CAUSE_WDT_IRQ:
            return 18UL;
        default:
            return 2UL;
    }
}

void delayOneMillisecond() {
    reg32(SYST_CSR) = 0UL;
    reg32(SYST_RVR) = (CPU_CLOCK_HZ / 1000UL) - 1UL;
    reg32(SYST_CVR) = 0UL;
    reg32(SYST_CSR) = SYST_ENABLE_CLKSOURCE;
    while ((reg32(SYST_CSR) & SYST_COUNTFLAG) == 0UL) {
    }
    reg32(SYST_CSR) = 0UL;
}

void delaySeconds(uint32_t seconds) {
    for (uint32_t ms = 0; ms < seconds * 1000UL; ++ms) {
        delayOneMillisecond();
    }
}

void reportCausePreinit() {
    const uint32_t cause = reg32(DIAG_CAUSE_ADDR);
    delaySeconds(encodedDelaySeconds(cause));
    reg32(DIAG_CAUSE_ADDR) = 0UL;
    reg32(POWER_GPREGRET) = DFU_MAGIC_SERIAL_ONLY_RESET;
    reg32(SCB_AIRCR) = SYSRESETREQ;
    while (true) {
    }
}

using PreinitFunc = void (*)();
__attribute__((section(".preinit_array"), used)) PreinitFunc g_reportCausePreinit = reportCausePreinit;
}

void setup() {
}

void loop() {
}
