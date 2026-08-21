// NrfPower.h - power management for nRF52840.
//
// Covers the practical sleep / wake / regulator surface of the POWER
// peripheral:
//   * System ON sleep via WFI / WFE (CPU off, peripherals running).
//   * Constant-latency vs low-power sub-mode (POWER_TASKS_CONSTLAT/_LOWPWR).
//   * DCDC regulator enable (~30% lower run/idle current at typical 3.3 V).
//   * RAM retention bitmap for System OFF (which 8 banks keep their contents).
//   * System OFF entry with wake-source configuration (GPIO DETECT, NFC field,
//     USB plug detect) and USB-safe no-return semantics.
//   * Read/clear POWER_RESETREAS so a sketch knows WHY it woke up.
//   * GPREGRET / GPREGRET2 - 8-bit retention registers that survive any reset
//     short of POR. The verified hands-free upload path already uses GPREGRET
//     for its bootloader-reset magic; the helpers here are read-only.
//
// SystemOFF caveats:
//   * SystemOFF is the lowest-power mode (~0.4 uA). enterSystemOff() DOES NOT
//     RETURN. If VBUS is already present, the safe default remains in System ON
//     sleep so USB enumeration and the 1200-bps recovery endpoint stay alive.
//     Pass allowUsbDisconnect=true only when losing USB is intentional.
//   * GPIO wake requires the SENSE field in GPIO_PIN_CNF to be set BEFORE
//     SystemOFF. enableGpioWake() does that for you.
//   * NFC wake requires the NFCT peripheral to be running in tag-emulation
//     mode. NfcTag::begin() is the easy way; without it, enableNfcWake() is
//     just a flag with nothing behind it.
//   * USB wake requires VBUS to be detected by POWER_USBREGSTATUS. Cables
//     pulled BEFORE SystemOFF won't wake on re-plug if VBUS was never seen.

#pragma once

#include <stdint.h>

class NrfPower {
public:
    // -- Reset reason bits (POWER_RESETREAS) --------------------------------
    // At boot the bits that fired since last clear are set; clear by writing
    // 1s to them. getResetReason() returns the raw value.
    static constexpr uint32_t RESET_PIN          = 1U << 0;   // RESET pin
    static constexpr uint32_t RESET_WATCHDOG     = 1U << 1;
    static constexpr uint32_t RESET_SOFT         = 1U << 2;   // SYSRESETREQ
    static constexpr uint32_t RESET_LOCKUP       = 1U << 3;
    static constexpr uint32_t RESET_GPIO_WAKE    = 1U << 16;  // DETECT
    static constexpr uint32_t RESET_LPCOMP_WAKE  = 1U << 17;
    static constexpr uint32_t RESET_DEBUGIF_WAKE = 1U << 18;
    static constexpr uint32_t RESET_NFC_WAKE     = 1U << 19;
    static constexpr uint32_t RESET_VBUS_WAKE    = 1U << 20;

    // -- "System ON" sub-modes ----------------------------------------------
    // The CPU still sleeps in WFI/WFE either way, but the regulator behavior
    // differs:
    //   MODE_LOW_POWER         - clocks ramp down during sleep. Lowest System-
    //                            ON current, +3 us wake latency. Right for
    //                            sketches that sleep most of the time.
    //   MODE_CONSTANT_LATENCY  - keeps high-speed clocks alive so wake is
    //                            deterministic (sub-us). Right for protocol
    //                            stacks that can't tolerate jitter.
    static constexpr uint8_t MODE_UNCHANGED         = 0U;
    static constexpr uint8_t MODE_LOW_POWER         = 1U;
    static constexpr uint8_t MODE_CONSTANT_LATENCY  = 2U;

    // -- System ON sleep ----------------------------------------------------
    // CPU sleeps until ANY enabled NVIC interrupt fires. Whatever peripheral
    // wakes us continues running (only the CPU was halted).
    static void sleep();

    // CPU sleeps until an SEV / external event. Pairs with peripherals that
    // emit events without a dedicated IRQ (PPI bridges).
    static void sleepWfe();

    // Cooperative blocking sleep for milliseconds. Programs RTC1 CC0 to fire
    // after delayMs, enters WFI, returns when CC0 (or any earlier IRQ) fires.
    // RTC1 is shared with NrfRtc - do not call from inside an RTC1 callback.
    static void sleepMs(uint32_t delayMs);

    static void setMode(uint8_t mode);

    // -- DCDC regulator -----------------------------------------------------
    // The main DCDC saves ~30% vs the LDO when VDD >= ~1.8 V. There is a
    // second high-voltage DCDC for boards driven from a battery direct; the
    // HV DCDC only makes sense if the board hardware actually routes the
    // LCx + DECx pins. Most clones DO NOT - check the schematic first or
    // you get a startup brown-out.
    static void enableDcdc(bool enable);
    static void enableHvDcdc(bool enable);
    static bool isDcdcEnabled();

    // -- RAM retention ------------------------------------------------------
    // nRF52840 has 9 RAM banks (R0..R7 + R8). bankBitmap bit i = bank i
    // retains its contents through SystemOFF. Default at boot is "all
    // retained"; only call this if you want to skip retaining some banks
    // (saves more power but loses stack / heap / .bss in those banks).
    static void setRamRetention(uint16_t bankBitmap);
    static uint16_t getRamRetention();

    // -- Wake sources for SystemOFF ----------------------------------------

    // Configure a GPIO as a SystemOFF wake source. activeHigh=true wakes on
    // HIGH, activeHigh=false wakes on LOW. The pin must be configured as
    // input (with optional pull) elsewhere - this only sets the SENSE field
    // in PIN_CNF.
    static void enableGpioWake(uint8_t pin, bool activeHigh);
    static void disableGpioWake(uint8_t pin);

    // NFC field detect - the NFCT peripheral must be running in tag mode
    // (NfcTag::begin()) for this to actually wake.
    static void enableNfcWake(bool enable);

    // USB plug detect via POWER_USBREGSTATUS.VBUSDETECT.
    static void enableUsbWake(bool enable);

    // -- SystemOFF entry ----------------------------------------------------
    // Enters SystemOFF. DOES NOT RETURN. With USB VBUS already present, the
    // default remains in interrupt-driven System ON sleep instead, preserving
    // enumeration and 1200-bps upload recovery. Set allowUsbDisconnect=true
    // only when the caller explicitly accepts losing the active USB session.
    static void enterSystemOff(bool allowUsbDisconnect = false) __attribute__((noreturn));

    // -- Reset reason / GPREGRET --------------------------------------------
    static uint32_t getResetReason();
    static void clearResetReason();
    static bool wokeFromSystemOff();   // any of the *_WAKE bits set

    static uint8_t getGpregret();
    static uint8_t getGpregret2();

    // -- Diagnostics --------------------------------------------------------
    static bool isUsbVbusPresent();
    static bool isUsbRegulatorOutputReady();
};
