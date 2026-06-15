#include <stdint.h>

extern "C" {
extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;
int main(void);
void __libc_init_array(void);
void Default_Handler(void);
void POWER_CLOCK_IRQHandler(void);
void RADIO_IRQHandler(void);
void UARTE0_UART0_IRQHandler(void);
void SPIM0_SPIS0_TWIM0_TWIS0_SPI0_TWI0_IRQHandler(void);
void SPIM1_SPIS1_TWIM1_TWIS1_SPI1_TWI1_IRQHandler(void);
void NFCT_IRQHandler(void);
void GPIOTE_IRQHandler(void);
void WDT_IRQHandler(void);
void USBD_IRQHandler(void);
// Bare-metal SoftDevice/MBR presence detection (NrfSoftDevice.cpp).  Stamps a
// persistent diag SRAM marker so a J-Link/SWD read confirms the core correctly
// detected whether a SoftDevice is in flash.  Freestanding-safe: literal
// addresses only, so it is valid to call before the C/C++ runtime is up.
void nrfSoftDeviceBootDetect(void);
}

using IsrVector = void (*)(void);
extern const IsrVector g_isrVectors[];

// Automatic rescue was intentionally removed for this board family to reduce
// uncertainty while debugging the bootloader/app hand-off. Startup now only
// performs the minimal vector-table re-anchor plus normal C/C++ runtime init.
namespace {
constexpr uint32_t kScbVtorAddr = 0xE000ED08UL;
constexpr uint32_t kScbIcsrAddr = 0xE000ED04UL;
constexpr uint32_t kScbCpacrAddr = 0xE000ED88UL;
constexpr uint32_t kScbIcsrPendstclr = (1UL << 25);
constexpr uint32_t kScbIcsrPendsvclr = (1UL << 27);
constexpr uint32_t kScbCpacrCp10Cp11FullAccess = (3UL << 20) | (3UL << 22);
constexpr uint32_t kNvicIcerBase = 0xE000E180UL;
constexpr uint32_t kNvicIcprBase = 0xE000E280UL;
constexpr uint32_t kNvicRegisterCount = 2UL;
constexpr uint32_t kSysTickCtrlAddr = 0xE000E010UL;
constexpr uint32_t kSysTickValAddr = 0xE000E018UL;
constexpr uint32_t kClockBase = 0x40000000UL;
constexpr uint32_t kClockEventsDone = 0x10CUL;
constexpr uint32_t kClockEventsCtto = 0x110UL;
constexpr uint32_t kClockCtiv = 0x538UL;

// WDT registers (nRF52840 PS §24.5 register map table)
// 0x000 = TASKS_START, 0x100 = EVENTS_TIMEOUT, 0x304/0x308 = INTENSET/INTENCLR
// 0x400 = RUNSTATUS (not 0x100!), 0x404 = REQSTATUS, 0x600..0x61C = RR[0..7]
constexpr uint32_t kWdtBase           = 0x40010000UL;
constexpr uint32_t kWdtEventsTimeout  = 0x100UL;
constexpr uint32_t kWdtRunStatus      = 0x400UL;   // RUNSTATUS — 1 if WDT is running
constexpr uint32_t kWdtReqStatus      = 0x404UL;   // REQSTATUS — bitmask of active RR channels
constexpr uint32_t kWdtRrBase         = 0x600UL;   // RR[0..7], stride 4
constexpr uint32_t kWdtRrMagic        = 0x6E524635UL;

// NRF52840 POWER.GPREGRET (0x4000051C) — magic written before SYSRESETREQ to
// tell the Adafruit-derived bootloader to stay in the configured recovery mode
// so the user (or upload tool) can see the device and reflash.
constexpr uint32_t kGpregretAddr      = 0x4000051CUL;
#if defined(NRF_SYSTEM_BOOTLOADER_RESET_MODE) && (NRF_SYSTEM_BOOTLOADER_RESET_MODE == 1)
constexpr uint32_t kGpregretRecoveryMagic = 0x4EUL; // DFU_MAGIC_SERIAL_ONLY_RESET
#else
constexpr uint32_t kGpregretRecoveryMagic = 0x57UL; // DFU_MAGIC_UF2_RESET
#endif

// Diagnostic marker written once at the very start of Reset_Handler.
// Stored in the SoftDevice-reserved SRAM zone (0x20000000-0x20005FFF), which
// is not touched by either the bootloader startup or our own linker script,
// so it survives every NVIC_SystemReset() round-trip. Reading this location
// from a running app confirms that Reset_Handler executed at least once.
constexpr uint32_t kDiagSramAddr      = 0x20004000UL;
constexpr uint32_t kDiagCauseSramAddr = 0x20004004UL;
constexpr uint32_t kDiagFaultInfoSramAddr = 0x20004008UL;  // VECTACTIVE (Default) or stacked PC (HardFault)
constexpr uint32_t kDiagResetHandlerMark = 0xA55A0001UL;  // app Reset_Handler ran
constexpr uint32_t kDiagCauseDefaultHandler = 0xCA5E0DEFUL;
constexpr uint32_t kDiagCauseHardFault = 0xCA5E0FA1UL;
constexpr uint32_t kDiagCauseWdtIrq = 0xCA5E0AD0UL;

// AIRCR — used by fault handlers to do a clean system reset instead of
// spinning forever, which would otherwise stall the WDT and block recovery.
constexpr uint32_t kAircrAddr         = 0xE000ED0CUL;
constexpr uint32_t kAircrSysresetReq  = 0x05FA0004UL;

inline volatile uint32_t &raw32(uint32_t address) {
    return *reinterpret_cast<volatile uint32_t *>(address);
}

void sanitizeExecutionState(void) {
    const uint32_t topOfStack = reinterpret_cast<uint32_t>(&_estack);
    __asm volatile(
        "msr msp, %[stack]\n"
        "movs r0, #0\n"
        "msr psp, r0\n"
        "msr control, r0\n"
        "msr basepri, r0\n"
        "dsb 0xF\n"
        "isb 0xF\n"
        :
        : [stack] "r"(topOfStack)
        : "r0", "memory");
}

void applySystemInitWorkarounds(void) {
    // Nordic errata 36: CLOCK.EVENTS_DONE / EVENTS_CTTO / CTIV may retain
    // stale bootloader values across a hand-off. Clear them so later clock
    // bring-up and timeout logic starts from a known reset-like state.
    raw32(kClockBase + kClockEventsDone) = 0UL;
    raw32(kClockBase + kClockEventsCtto) = 0UL;
    raw32(kClockBase + kClockCtiv) = 0UL;

#if defined(__FPU_USED) && (__FPU_USED == 1)
    raw32(kScbCpacrAddr) |= kScbCpacrCp10Cp11FullAccess;
    __asm volatile("dsb 0xF" ::: "memory");
    __asm volatile("isb 0xF" ::: "memory");
#endif
}

void sanitizeInterruptState(void) {
    // Adafruit-style bootloader hand-off is an app jump, not a guaranteed chip
    // reset. Clear any IRQ enables/pending bits and stop SysTick so leftover
    // bootloader state cannot vector into our Default_Handler before init().
    for (uint32_t index = 0; index < kNvicRegisterCount; ++index) {
        const uint32_t offset = index * sizeof(uint32_t);
        raw32(kNvicIcerBase + offset) = 0xFFFFFFFFUL;
        raw32(kNvicIcprBase + offset) = 0xFFFFFFFFUL;
    }
    raw32(kSysTickCtrlAddr) = 0UL;
    raw32(kSysTickValAddr) = 0UL;
    raw32(kScbIcsrAddr) = kScbIcsrPendstclr | kScbIcsrPendsvclr;
}

// Feed any WDT channels that are active.  Called once during startup so that
// the initial C++ runtime setup (which can take tens of milliseconds) does not
// trip a short bootloader WDT.  On nRF52840 the WDT cannot be stopped once
// started, so the only safe action is to feed it.
void feedBootloaderWdt(void) {
    if ((raw32(kWdtBase + kWdtRunStatus) & 1UL) == 0UL) {
        return;  // WDT not running — nothing to do
    }

    // REQSTATUS is not a reliable "enabled channel" mask to poll at arbitrary
    // times during app runtime. If the bootloader left WDT running, reload all
    // channels; writes to disabled RR registers are ignored by hardware.
    for (uint32_t ch = 0; ch < 8UL; ++ch) {
        raw32(kWdtBase + kWdtRrBase + ch * 4UL) = kWdtRrMagic;
    }
}
}

extern "C" void nrfWdtFeed(void) {
    feedBootloaderWdt();
}

extern "C" void WDT_IRQHandler(void) {
    raw32(kDiagCauseSramAddr) = kDiagCauseWdtIrq;
    raw32(kWdtBase + kWdtEventsTimeout) = 0UL;
    feedBootloaderWdt();
}

extern "C" void nrfMarkBootHealthy(void) {
}

extern "C" uint8_t nrfBootAttemptCount(void) {
    return 0U;
}

extern "C" void Default_Handler(void) {
    // Reset rather than spin — spinning blocks the WDT and makes recovery
    // impossible on boards where the WDT was started by the bootloader.
    // Write bootloader magic first so the Adafruit-derived bootloader
    // stays in the configured recovery mode instead of
    // jumping directly back to the crashing application and creating a tight
    // reset loop where USB never has time to enumerate.
    // Capture WHICH exception/IRQ landed here (SCB->ICSR VECTACTIVE, bits 0..8)
    // into the bootloader-persistent diag slot so a post-reset SWD read names
    // the offending vector. For a peripheral IRQ, IRQn = VECTACTIVE - 16.
    raw32(kDiagFaultInfoSramAddr) =
        (*reinterpret_cast<volatile uint32_t *>(0xE000ED04UL)) & 0x1FFUL;
    raw32(kDiagCauseSramAddr) = kDiagCauseDefaultHandler;
    raw32(kGpregretAddr) = kGpregretRecoveryMagic;
    raw32(kAircrAddr) = kAircrSysresetReq;
    while (true) { }
}

// NrfGdbStub.cpp provides its own HardFault_Handler when the GDB stub is
// active.  Guard against the linker seeing two strong definitions.
#if !defined(NRF_SYSTEM_USB_GDB_STUB) || (NRF_SYSTEM_USB_GDB_STUB == 0)
extern "C" void HardFault_Handler(void) {
    // Write a distinct marker before resetting so the next app run can detect
    // that a HardFault occurred (read 0x20004002 for the fault flag byte).
    *reinterpret_cast<volatile uint32_t *>(kDiagSramAddr) =
        (*reinterpret_cast<volatile uint32_t *>(kDiagSramAddr) & 0xFFFF0000UL) | 0x0000FA1UL;
    // Capture the stacked faulting PC (exception frame offset 0x18) into the
    // persistent diag slot so a post-reset SWD read points at the instruction
    // that faulted. Assumes the fault was taken from an MSP context (the
    // bare-metal Arduino case - no PSP threads).
    {
        uint32_t msp;
        __asm volatile("mrs %0, msp" : "=r"(msp));
        raw32(kDiagFaultInfoSramAddr) = reinterpret_cast<volatile uint32_t *>(msp)[6];
    }
    raw32(kDiagCauseSramAddr) = kDiagCauseHardFault;
    raw32(kGpregretAddr) = kGpregretRecoveryMagic;
    raw32(kAircrAddr) = kAircrSysresetReq;
    while (true) { }
}
#endif  // !NRF_SYSTEM_USB_GDB_STUB

extern "C" void Reset_Handler(void) {
    __asm volatile("cpsid i" ::: "memory");

    // Clone bootloaders do not always re-enter the application through a full
    // hardware reset path. Reassert the reset-time thread context (MSP, no PSP,
    // privileged Thread mode, no BASEPRI masking) before touching C runtime
    // state so an app jump behaves like a real reset.
    sanitizeExecutionState();
    applySystemInitWorkarounds();

    // Write a diagnostic marker to a SRAM location that is not touched by
    // either the bootloader startup or our own linker script (SoftDevice-
    // reserved zone, 0x20000000–0x20005FFF).  This allows a subsequent app
    // run to detect whether this Reset_Handler ever executed.
    *reinterpret_cast<volatile uint32_t *>(kDiagSramAddr) = kDiagResetHandlerMark;

    // If the bootloader started its WDT before jumping to the app it cannot
    // be stopped on nRF52840 (PS §24.3).  Feed every active channel once here
    // to survive the C/C++ runtime initialisation window.
    feedBootloaderWdt();

    // Bootloader hand-off does not guarantee that VTOR already points at the
    // application's vector table. Re-anchor it immediately so any early fault,
    // SysTick, or peripheral interrupt resolves against this image instead of a
    // stale bootloader/MBR table.
    raw32(kScbVtorAddr) = reinterpret_cast<uint32_t>(&g_isrVectors[0]);
    __asm volatile("dsb 0xF" ::: "memory");
    __asm volatile("isb 0xF" ::: "memory");
    sanitizeInterruptState();

    // Record whether an MBR + SoftDevice image is present in flash.  The core
    // never enables it (all wireless stacks own the radio bare-metal), so this
    // is purely awareness: a dormant SoftDevice keeps every peripheral free for
    // the application.  The marker lands in a persistent diag SRAM slot so it
    // can be read back over SWD on a board with no serial console.
    nrfSoftDeviceBootDetect();

    uint32_t *source = &_sidata;
    uint32_t *destination = &_sdata;
    while (destination < &_edata) {
        *destination++ = *source++;
    }

    destination = &_sbss;
    while (destination < &_ebss) {
        *destination++ = 0;
    }

    __libc_init_array();
    __asm volatile("cpsie i" ::: "memory");
    main();
    while (true) {
    }
}

extern "C" void NMI_Handler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void HardFault_Handler(void);  // Defined above — resets on fault
extern "C" void MemManage_Handler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void BusFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void SVC_Handler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void DebugMon_Handler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void PendSV_Handler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void SysTick_Handler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void POWER_CLOCK_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void RADIO_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void UARTE0_UART0_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void SPIM0_SPIS0_TWIM0_TWIS0_SPI0_TWI0_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void SPIM1_SPIS1_TWIM1_TWIS1_SPI1_TWI1_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void NFCT_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void GPIOTE_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void WDT_IRQHandler(void);
extern "C" void USBD_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void RTC0_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void RTC1_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void RTC2_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void TIMER0_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void TIMER1_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void TIMER2_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void TIMER3_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void TIMER4_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void COMP_LPCOMP_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void SWI0_EGU0_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void SWI1_EGU1_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void SWI2_EGU2_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void SWI3_EGU3_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void SWI4_EGU4_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void SWI5_EGU5_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
extern "C" void MWU_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));

// Cortex-M4 vector table for nRF52840.
//   Index   0     = initial main stack pointer.
//   Indices 1..15 = system exception handlers.
//   Index (16 + IRQn) = peripheral IRQ handlers per the nRF52840 PS.
// IRQs 7..38 (SAADC..FPU) are placed as Default_Handler stubs so that
// USBD lands at the correct slot 16 + 39 = 55.
// __attribute__((used)) prevents the C++ optimizer from discarding the array
// before the linker even sees it. Without it, --gc-sections + dead-store
// elimination drops g_isrVectors entirely and the chip jumps into garbage at
// the FLASH origin on reset.
__attribute__((section(".isr_vector"), used)) const IsrVector g_isrVectors[] = {
    reinterpret_cast<IsrVector>(&_estack),
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
    MemManage_Handler,
    BusFault_Handler,
    UsageFault_Handler,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    SVC_Handler,
    DebugMon_Handler,
    nullptr,
    PendSV_Handler,
    SysTick_Handler,
    POWER_CLOCK_IRQHandler,                       // IRQ  0
    RADIO_IRQHandler,                             // IRQ  1
    UARTE0_UART0_IRQHandler,                      // IRQ  2
    SPIM0_SPIS0_TWIM0_TWIS0_SPI0_TWI0_IRQHandler, // IRQ  3
    SPIM1_SPIS1_TWIM1_TWIS1_SPI1_TWI1_IRQHandler, // IRQ  4
    NFCT_IRQHandler,                              // IRQ  5
    GPIOTE_IRQHandler,                            // IRQ  6
    Default_Handler,                              // IRQ  7 SAADC
    TIMER0_IRQHandler,                            // IRQ  8 TIMER0
    TIMER1_IRQHandler,                            // IRQ  9 TIMER1
    TIMER2_IRQHandler,                            // IRQ 10 TIMER2
    RTC0_IRQHandler,                              // IRQ 11 RTC0
    Default_Handler,                              // IRQ 12 TEMP
    Default_Handler,                              // IRQ 13 RNG
    Default_Handler,                              // IRQ 14 ECB
    Default_Handler,                              // IRQ 15 CCM_AAR
    WDT_IRQHandler,                              // IRQ 16 WDT
    RTC1_IRQHandler,                              // IRQ 17 RTC1
    Default_Handler,                              // IRQ 18 QDEC
    COMP_LPCOMP_IRQHandler,                       // IRQ 19 COMP_LPCOMP
    SWI0_EGU0_IRQHandler,                         // IRQ 20 SWI0_EGU0
    SWI1_EGU1_IRQHandler,                         // IRQ 21 SWI1_EGU1
    SWI2_EGU2_IRQHandler,                         // IRQ 22 SWI2_EGU2
    SWI3_EGU3_IRQHandler,                         // IRQ 23 SWI3_EGU3
    SWI4_EGU4_IRQHandler,                         // IRQ 24 SWI4_EGU4
    SWI5_EGU5_IRQHandler,                         // IRQ 25 SWI5_EGU5
    TIMER3_IRQHandler,                            // IRQ 26 TIMER3
    TIMER4_IRQHandler,                            // IRQ 27 TIMER4
    Default_Handler,                              // IRQ 28 PWM0
    Default_Handler,                              // IRQ 29 PDM
    Default_Handler,                              // IRQ 30 NVMC
    Default_Handler,                              // IRQ 31 PPI
    MWU_IRQHandler,                               // IRQ 32 MWU
    Default_Handler,                              // IRQ 33 PWM1
    Default_Handler,                              // IRQ 34 PWM2
    Default_Handler,                              // IRQ 35 SPIM2_SPIS2_SPI2
    RTC2_IRQHandler,                              // IRQ 36 RTC2
    Default_Handler,                              // IRQ 37 I2S
    Default_Handler,                              // IRQ 38 FPU
    USBD_IRQHandler,                              // IRQ 39 USBD
};
