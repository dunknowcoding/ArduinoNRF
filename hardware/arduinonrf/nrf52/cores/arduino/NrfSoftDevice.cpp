// NrfSoftDevice.cpp - implementation of the SoftDevice/MBR awareness layer.
// See NrfSoftDevice.h for the architecture rationale.

#include "NrfSoftDevice.h"

namespace {
// Tracks whether requestEnable() ever succeeded.  The bare-metal core leaves
// this false for the entire run; only the guarded opt-in hook can set it.
bool g_softDeviceEnabled = false;

inline volatile uint32_t &raw32(uint32_t address) {
    return *reinterpret_cast<volatile uint32_t *>(address);
}
}  // namespace

bool NrfSoftDevice::isPresent() {
    using namespace nrf_sd_detail;
    return raw32(kInfoBase + kMagicOffset) == kMagicValue;
}

NrfSoftDevice::Status NrfSoftDevice::status() {
    if (!isPresent()) {
        return Status::Absent;
    }
    return g_softDeviceEnabled ? Status::Enabled : Status::Dormant;
}

uint32_t NrfSoftDevice::baseAddress() {
    return nrf_sd_detail::kMbrSize;
}

uint32_t NrfSoftDevice::appStartAddress() {
    using namespace nrf_sd_detail;
    if (!isPresent()) {
        // No SoftDevice: the application is linked at the MBR-less flash origin.
        return 0x1000UL;
    }
    // SD_SIZE field is the total size from address 0 (MBR + SoftDevice), i.e.
    // exactly the application start address (S140 v6.1.1 -> 0x26000).
    return raw32(kInfoBase + kSizeOffset);
}

uint32_t NrfSoftDevice::firmwareId() {
    using namespace nrf_sd_detail;
    if (!isPresent()) {
        return 0UL;
    }
    return raw32(kInfoBase + kFwidOffset) & 0x0000FFFFUL;
}

uint32_t NrfSoftDevice::versionRaw() {
    using namespace nrf_sd_detail;
    if (!isPresent()) {
        return 0UL;
    }
    return raw32(kInfoBase + kVersionOffset);
}

uint32_t NrfSoftDevice::infoWord(uint32_t byteOffset) {
    return raw32(nrf_sd_detail::kInfoBase + byteOffset);
}

void NrfSoftDevice::ensureBareMetalReady() {
    // The core never enables a SoftDevice, so nothing to release: RADIO,
    // TIMER0, RTC0, CCM/AAR/ECB and the CLOCK arbitration are all under
    // application control from reset.  Just refresh the boot presence marker so
    // a tool re-reading SRAM after the sketch started sees a current value.
    nrfSoftDeviceBootDetect();
}

extern "C" __attribute__((weak)) bool nrfSoftDeviceEnableImpl(bool /*enable*/) {
    // Default: no Nordic SDK vendored, so there is no SoftDevice SVC interface
    // to call.  A user who wants the certified BLE stack overrides this.
    return false;
}

bool NrfSoftDevice::requestEnable() {
#if defined(NRF_ENABLE_SOFTDEVICE) && (NRF_ENABLE_SOFTDEVICE == 1)
    if (!isPresent()) {
        return false;
    }
    const bool ok = nrfSoftDeviceEnableImpl(true);
    if (ok) {
        g_softDeviceEnabled = true;
    }
    return ok;
#else
    // Guard: enabling a SoftDevice is mutually exclusive with NimBLE/Thread and
    // is therefore opt-in at build time only.
    return false;
#endif
}

bool NrfSoftDevice::requestDisable() {
    const bool ok = nrfSoftDeviceEnableImpl(false);
    if (ok) {
        g_softDeviceEnabled = false;
    }
    return ok;
}

extern "C" void nrfSoftDeviceBootDetect(void) {
    using namespace nrf_sd_detail;
    if (raw32(kInfoBase + kMagicOffset) == kMagicValue) {
        raw32(kDiagSdMarkerAddr) = kDiagSdPresentBase | (raw32(kInfoBase + kFwidOffset) & 0xFFFFUL);
        raw32(kDiagSdAppStartAddr) = raw32(kInfoBase + kSizeOffset);
    } else {
        raw32(kDiagSdMarkerAddr) = kDiagSdAbsentMark;
        raw32(kDiagSdAppStartAddr) = 0x00001000UL;
    }
}
