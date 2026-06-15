// NrfSoftDevice.h - Nordic SoftDevice / MBR awareness for the bare-metal
// ArduinoNRF core.
//
// THE ARCHITECTURE, IN ONE PARAGRAPH.  This core is deliberately SoftDevice-
// free.  Every wireless stack in the package owns the radio directly: NimBLE
// runs its own link-layer controller, OpenThread runs on the package's own
// bare-metal 802.15.4 RADIO driver (no nrfx, no SoftDevice), and Zigbee talks
// to an external CC2530 transceiver.  None of them use a Nordic SoftDevice, so
// the core never calls sd_softdevice_enable().  Enabling a SoftDevice would
// seize RADIO, TIMER0, RTC0, the CCM/AAR/ECB crypto blocks and the CLOCK
// arbitration, and would break all three stacks at once.  On the ProMicro
// nRF52840 clones there isn't even a SoftDevice in flash (the application is
// linked at 0x1000 with no SoftDevice region); only the nice!nano carries a
// dormant S140.
//
// This module does NOT start a SoftDevice.  It makes the core SOFTDEVICE-AWARE
// and SAFE:
//   * detect at boot whether an MBR + SoftDevice image is present in flash,
//   * guarantee the application keeps full ownership of every peripheral a
//     SoftDevice would otherwise reserve (the SoftDevice stays DORMANT - it is
//     never enabled, so it never claims a single peripheral),
//   * expose the SoftDevice's identity (application start address, firmware id,
//     version) so sketches and tools can reason about the flash layout,
//   * provide a GUARDED opt-in enable hook for the rare advanced user who
//     vendors the Nordic SDK and explicitly wants the certified BLE stack
//     INSTEAD of NimBLE/Thread (off by default; requires -DNRF_ENABLE_SOFTDEVICE
//     AND a strong nrfSoftDeviceEnableImpl() to be linked in).
//
// See docs/platform/SOFTDEVICE.md for the full rationale and the upgrade path.

#pragma once

#include <stdint.h>

namespace nrf_sd_detail {
// nRF52 SoftDevice/MBR flash layout (Nordic nrf_sdm.h / nrf_mbr.h).  The MBR
// occupies the first page; the SoftDevice image starts immediately after it, so
// the SoftDevice base address equals MBR_SIZE.  The SoftDevice information
// struct sits at SoftDevice base + 0x2000.
constexpr uint32_t kMbrSize          = 0x1000UL;                       // SoftDevice base
constexpr uint32_t kInfoStructOffset = 0x2000UL;
constexpr uint32_t kInfoBase         = kMbrSize + kInfoStructOffset;   // 0x3000
constexpr uint32_t kMagicOffset      = 0x04UL;                         // -> 0x3004
constexpr uint32_t kSizeOffset       = 0x08UL;                         // -> 0x3008 = app start
constexpr uint32_t kFwidOffset       = 0x0CUL;                         // -> 0x300C (low 16b)
constexpr uint32_t kVersionOffset    = 0x14UL;                         // -> 0x3014
constexpr uint32_t kMagicValue       = 0x51B1E5DBUL;                   // SD_MAGIC_NUMBER

// Persistent diagnostic SRAM scratch (inside the SoftDevice-reserved zone
// 0x20000000-0x20005FFF that neither the bootloader nor our linker script
// touches, so it survives every reset).  nrfSoftDeviceBootDetect() stamps the
// boot-time presence result here so a J-Link/SWD read confirms detection ran
// without needing a serial console.  0x20004000/4/8 are already used by the
// startup diag markers; these two words extend that block.
constexpr uint32_t kDiagSdMarkerAddr   = 0x2000400CUL;
constexpr uint32_t kDiagSdAppStartAddr = 0x20004010UL;
constexpr uint32_t kDiagSdPresentBase  = 0x5D000000UL;   // | (fwid & 0xFFFF)
constexpr uint32_t kDiagSdAbsentMark   = 0x5DAB0000UL;   // "5D ABsent" - ran, none found
}  // namespace nrf_sd_detail

class NrfSoftDevice {
public:
    enum class Status : uint8_t {
        Absent = 0,   // no MBR/SoftDevice in flash (ProMicro clones)
        Dormant,      // SoftDevice present in flash but never enabled (bare-metal)
        Enabled,      // SoftDevice enabled via the guarded opt-in hook
    };

    // -- presence ----------------------------------------------------------
    // True when a valid SoftDevice information struct (magic 0x51B1E5DB) is
    // found at 0x3004.  Reads flash directly; safe to call at any time.
    static bool isPresent();
    static Status status();

    // -- identity (meaningful only when isPresent()) -----------------------
    static uint32_t baseAddress();      // = MBR_SIZE (0x1000), the SoftDevice base
    static uint32_t appStartAddress();  // SD_SIZE field - where the application lives
    static uint32_t firmwareId();       // SD_FWID, low 16 bits (e.g. S140 v6 -> 0x00B6)
    static uint32_t versionRaw();       // SD_VERSION (major*1e6 + minor*1e3 + revision)
    static uint32_t infoWord(uint32_t byteOffset);  // raw info-struct reader

    // -- bare-metal safety -------------------------------------------------
    // Because the core never enables a SoftDevice, every peripheral a
    // SoftDevice would reserve (RADIO, TIMER0, RTC0, CCM/AAR/ECB, CLOCK
    // arbitration) stays owned by the application from reset - which is exactly
    // what NimBLE and Thread require.  This call refreshes the boot presence
    // marker and is idempotent; Reset_Handler invokes it once, and a sketch may
    // call it again with no side effects.
    static void ensureBareMetalReady();

    // -- guarded opt-in enable hook ----------------------------------------
    // OFF BY DEFAULT and mutually exclusive with NimBLE/Thread.  Returns false
    // unless the firmware was built with -DNRF_ENABLE_SOFTDEVICE AND a strong
    // nrfSoftDeviceEnableImpl() (from a vendored Nordic SDK) is linked in.
    // requestDisable() always routes to the impl hook so a user stack can be
    // torn down again.  See docs/platform/SOFTDEVICE.md.
    static bool requestEnable();
    static bool requestDisable();
};

// Weak extension point.  The default implementation returns false (no Nordic
// SDK vendored).  A user who wants the certified SoftDevice BLE stack provides
// a strong definition that calls sd_softdevice_enable()/sd_softdevice_disable().
extern "C" bool nrfSoftDeviceEnableImpl(bool enable);

// Boot-time presence detection, called from Reset_Handler.  Writes the presence
// marker + detected application start address to the persistent diag SRAM slots.
// Freestanding-safe: touches only literal addresses, no globals, so it may run
// before the C/C++ runtime (.data/.bss) is initialised.
extern "C" void nrfSoftDeviceBootDetect(void);
