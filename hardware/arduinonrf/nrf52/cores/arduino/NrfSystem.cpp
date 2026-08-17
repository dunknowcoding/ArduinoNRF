#include "NrfSystem.h"

#include "Arduino.h"
#include "NrfBoard.h"

namespace {
// USB device identity comes from each board recipe. Runtime firmware does not
// have to reuse the bootloader PID: keeping application and bootloader
// identities distinct gives the host a clean PnP transition and prevents an
// MSC bootloader interface from being cached as a runtime CDC interface.
constexpr uint16_t kAdafruitVid = 0x239A;
constexpr uint8_t kAdafruitSerialOnlyResetMagic = 0x4EU;
constexpr uint8_t kAdafruitUf2ResetMagic = 0x57U;
constexpr uint32_t kPowerBase = 0x40000000UL;
constexpr uint32_t kPowerGpregret = 0x51CUL;
constexpr uint32_t kDfuDoubleResetMagic = 0x005A1AD5UL;
constexpr uint32_t kDfuDoubleResetMemAddr = 0x20007F7CUL;

uint8_t configuredBootloaderResetMagic() {
#if defined(NRF_SYSTEM_BOOTLOADER_RESET_MODE) && (NRF_SYSTEM_BOOTLOADER_RESET_MODE == 1)
    return kAdafruitSerialOnlyResetMagic;
#elif defined(NRF_SYSTEM_BOOTLOADER_RESET_MODE) && (NRF_SYSTEM_BOOTLOADER_RESET_MODE == 2)
    return kAdafruitUf2ResetMagic;
#elif defined(NRF_SYSTEM_BOOTLOADER_MODE) && (NRF_SYSTEM_BOOTLOADER_MODE == 1)
    return kAdafruitSerialOnlyResetMagic;
#else
    return kAdafruitUf2ResetMagic;
#endif
}

inline volatile uint32_t &reg32(uint32_t base, uint32_t offset) {
    return *reinterpret_cast<volatile uint32_t *>(base + offset);
}

inline volatile uint32_t &mem32(uint32_t address) {
    return *reinterpret_cast<volatile uint32_t *>(address);
}

inline void syncResetRequestWrites() {
    __asm volatile("dsb 0xF" ::: "memory");
    __asm volatile("isb 0xF" ::: "memory");
}

bool bootloaderDoubleResetFallbackEnabled() {
#if defined(NRF_SYSTEM_BOOTLOADER_DBL_RESET_FALLBACK) && (NRF_SYSTEM_BOOTLOADER_DBL_RESET_FALLBACK == 1)
    return true;
#else
    return false;
#endif
}

inline uint16_t boardVid() {
#if defined(NRF_SYSTEM_RUNTIME_USB_VID)
    return static_cast<uint16_t>(NRF_SYSTEM_RUNTIME_USB_VID);
#elif defined(ARDUINO_NRF52_PROMICRO) \
 || defined(ARDUINO_NRF52_NICENANO_V2) \
 || defined(ARDUINO_NRF52_SUPERMINI) \
 || defined(ARDUINO_NRF52_NRFMICRO) \
 || defined(ARDUINO_NRF52_MINI) \
 || defined(ARDUINO_NRF52_XIAO) \
 || defined(ARDUINO_NRF52_PITAYA_GO) \
 || defined(ARDUINO_NRF52_USB_DONGLE)
    return kAdafruitVid;
#else
    return 0x1915U;
#endif
}

// The system profile intentionally derives its answers from build flags rather
// than probing transport state at runtime. This keeps smoke tests deterministic
// and makes the package's declared upload/debug contract auditable from source.
NrfSerialTopology detectSerialTopology() {
#if defined(NRF_SYSTEM_DEFAULT_SERIAL_USB) && (NRF_SYSTEM_DEFAULT_SERIAL_USB == 1)
    return NrfSerialTopology::NativeUsbCdc;
#else
    return NrfSerialTopology::UartOnly;
#endif
}

NrfRuntimeUsbMode detectRuntimeUsbMode() {
#if defined(NRF_SYSTEM_HAS_USB_CDC) && (NRF_SYSTEM_HAS_USB_CDC == 1)
    return NrfRuntimeUsbMode::Cdc;
#else
    return NrfRuntimeUsbMode::Disabled;
#endif
}

NrfBootloaderInterface detectBootloaderInterface() {
#if defined(NRF_SYSTEM_BOOTLOADER_MODE) && (NRF_SYSTEM_BOOTLOADER_MODE == 2)
    return NrfBootloaderInterface::UsbUf2;
#elif defined(NRF_SYSTEM_BOOTLOADER_MODE) && (NRF_SYSTEM_BOOTLOADER_MODE == 1)
    return NrfBootloaderInterface::UsbDfu;
#elif defined(NRF_SYSTEM_USB_UPLOAD_PREFERRED) && (NRF_SYSTEM_USB_UPLOAD_PREFERRED == 1)
    return NrfBootloaderInterface::UsbDfu;
#else
    return NrfBootloaderInterface::None;
#endif
}

NrfMonitorTransport detectMonitorTransport() {
#if defined(NRF_SYSTEM_DEFAULT_SERIAL_USB) && (NRF_SYSTEM_DEFAULT_SERIAL_USB == 1)
    return NrfMonitorTransport::UsbCdc;
#else
    return NrfMonitorTransport::ExternalUart;
#endif
}

NrfUploadTransport detectUploadTransport() {
#if defined(NRF_SYSTEM_USB_UPLOAD_PREFERRED) && (NRF_SYSTEM_USB_UPLOAD_PREFERRED == 1)
    return NrfUploadTransport::UsbBootloaderOrSwd;
#else
    return NrfUploadTransport::SwdOpenOcd;
#endif
}

NrfUploadTrigger detectUploadTrigger() {
#if defined(NRF_SYSTEM_USB_UPLOAD_PREFERRED) && (NRF_SYSTEM_USB_UPLOAD_PREFERRED == 1)
    return NrfUploadTrigger::Touch1200;
#elif defined(NRF_SYSTEM_HAS_SWD) && (NRF_SYSTEM_HAS_SWD == 1)
    return NrfUploadTrigger::ExternalProbe;
#else
    return NrfUploadTrigger::None;
#endif
}

NrfDebugTransport detectDebugTransport() {
#if defined(NRF_SYSTEM_USB_GDB_STUB) && (NRF_SYSTEM_USB_GDB_STUB == 1)
        return NrfDebugTransport::UsbCdcGdbStub;
#elif defined(NRF_SYSTEM_HAS_SWD) && (NRF_SYSTEM_HAS_SWD == 1)
  #if defined(NRF_SYSTEM_DEBUG_PADS_ONLY) && (NRF_SYSTEM_DEBUG_PADS_ONLY == 1)
    return NrfDebugTransport::SwdPads;
  #else
    return NrfDebugTransport::SwdHeader;
  #endif
#else
    return NrfDebugTransport::None;
#endif
}

NrfUsbBackend detectUsbBackend() {
#if !defined(NRF_SYSTEM_HAS_USB_CDC) || (NRF_SYSTEM_HAS_USB_CDC == 0)
    return NrfUsbBackend::Disabled;
#elif defined(NRF_SYSTEM_USB_BACKEND) && (NRF_SYSTEM_USB_BACKEND == 2)
    return NrfUsbBackend::Auto;
#else
    return NrfUsbBackend::Custom;
#endif
}

NrfStorageBackend detectStorageBackend() {
#if defined(NRF_SYSTEM_STORAGE_BACKEND) && (NRF_SYSTEM_STORAGE_BACKEND == 2)
    return NrfStorageBackend::LogStructured;
#else
    return NrfStorageBackend::LegacySinglePage;
#endif
}

uint32_t storageReservedBytes() {
#if defined(NRF_STORAGE_REGION_BYTES)
    return static_cast<uint32_t>(NRF_STORAGE_REGION_BYTES);
#else
    return 0U;
#endif
}

uint32_t programFlashBytes() {
#if defined(NRF_BOARD_PROGRAM_FLASH_BYTES)
    return static_cast<uint32_t>(NRF_BOARD_PROGRAM_FLASH_BYTES);
#else
    return 0U;
#endif
}

uint32_t dataRamBytes() {
#if defined(NRF_BOARD_DATA_RAM_BYTES)
    return static_cast<uint32_t>(NRF_BOARD_DATA_RAM_BYTES);
#else
    return 0U;
#endif
}

NrfFlashProfile detectFlashProfile() {
#if defined(NRF_BOARD_FLASH_PROFILE) && (NRF_BOARD_FLASH_PROFILE == 1)
    return NrfFlashProfile::InternalFlash512K;
#elif defined(NRF_BOARD_FLASH_PROFILE) && (NRF_BOARD_FLASH_PROFILE == 2)
    return NrfFlashProfile::InternalFlash1M;
#elif defined(NRF_BOARD_FLASH_PROFILE) && (NRF_BOARD_FLASH_PROFILE == 3)
    return NrfFlashProfile::InternalFlash1MWithQspiUnknown;
#elif defined(NRF_BOARD_FLASH_PROFILE) && (NRF_BOARD_FLASH_PROFILE == 4)
    return NrfFlashProfile::InternalFlash1MWithQspi2M;
#elif defined(NRF_BOARD_FLASH_PROFILE) && (NRF_BOARD_FLASH_PROFILE == 5)
    return NrfFlashProfile::InternalFlash1MWithQspi8M;
#else
    return NrfFlashProfile::Unspecified;
#endif
}

NrfRamProfile detectRamProfile() {
#if defined(NRF_BOARD_RAM_PROFILE) && (NRF_BOARD_RAM_PROFILE == 1)
    return NrfRamProfile::InternalSram128K;
#elif defined(NRF_BOARD_RAM_PROFILE) && (NRF_BOARD_RAM_PROFILE == 2)
    return NrfRamProfile::InternalSram256K;
#else
    return NrfRamProfile::Unspecified;
#endif
}

uint8_t dfuAltSetting() {
#if defined(ARDUINO_NRF52_DEVBOARD_833)
    return 0xFFU;
#else
    return 0U;
#endif
}

bool uploadTouch1200Declared() {
#if defined(ARDUINO_NRF52_DEVBOARD_833)
    return false;
#else
    return true;
#endif
}

bool uploadTouch1200Verified() {
    return nrfBoardUploadProfileVerified();
}

bool uf2UploadSupported() {
    return detectBootloaderInterface() == NrfBootloaderInterface::UsbUf2;
}

bool uf2UploadNeeded() {
    return detectBootloaderInterface() == NrfBootloaderInterface::UsbUf2;
}

bool dfuUtilArgsBoardSpecific() {
    return detectBootloaderInterface() == NrfBootloaderInterface::UsbDfu;
}

uint16_t boardPid() {
#if defined(NRF_SYSTEM_RUNTIME_USB_PID)
    return static_cast<uint16_t>(NRF_SYSTEM_RUNTIME_USB_PID);
#endif
    // Runtime PID choices follow the same bootloader family but keep the
    // application distinct from the bootloader where that family does so
    // upstream. For the 0x00B3 nice!nano-clone bootloader, mature cores expect
    // the user firmware to enumerate as 0x00B4 rather than staying on the
    // bootloader PID. Keeping both sides on 0x00B3 made Windows-side hand-off
    // analysis ambiguous and removed the clean bootloader/app identity split.
#if defined(ARDUINO_NRF52_PROMICRO)
    return 0x00B4;
#elif defined(ARDUINO_NRF52_NICENANO_V2)
    return 0x0029;   // Adafruit UF2 fork, nice!nano v2 build
#elif defined(ARDUINO_NRF52_SUPERMINI)
    return 0x0029;   // Adafruit UF2 fork, nice!nano-compatible
#elif defined(ARDUINO_NRF52_NRFMICRO)
    return 0x0029;   // Adafruit UF2 fork, nice!nano-compatible
#elif defined(ARDUINO_NRF52_MINI)
    return 0x0029;
#elif defined(ARDUINO_NRF52_XIAO)
    return 0x0029;
#elif defined(ARDUINO_NRF52_DEVBOARD_833)
    return 0x5233;   // No native USB on nRF52833 build, kept for compat.
#elif defined(ARDUINO_NRF52_PITAYA_GO)
    return 0x0029;
#elif defined(ARDUINO_NRF52_USB_DONGLE)
    return 0x0029;
#else
    return 0x0029;
#endif
}

const char *defaultDebugProbe() {
#if defined(NRF_SYSTEM_USB_GDB_STUB) && (NRF_SYSTEM_USB_GDB_STUB == 1)
    return "USB CDC GDB stub";
#elif defined(ARDUINO_NRF52_PROMICRO)
    return "CMSIS-DAP or J-Link via SWD pads";
#elif defined(ARDUINO_NRF52_USB_DONGLE)
    return "CMSIS-DAP pogo-pin";
#elif defined(ARDUINO_NRF52_NICENANO_V2) || defined(ARDUINO_NRF52_SUPERMINI) || defined(ARDUINO_NRF52_NRFMICRO)
    return "CMSIS-DAP or DFU bootloader";
#else
    return "CMSIS-DAP";
#endif
}
}

const NrfSystemProfile &nrfSystemProfile() {
    // Cache the derived profile once because it depends entirely on compile-time
    // variant flags and board metadata, not on mutable peripheral state.
    static const NrfSystemProfile profile = {
        nrfBoardName(),
#ifdef USB_PRODUCT
        USB_PRODUCT,
#else
    "Arduino NRF52",
#endif
        defaultDebugProbe(),
        boardVid(),
        boardPid(),
        PIN_SERIAL_RX,
        PIN_SERIAL_TX,
#if defined(NRF_SYSTEM_HAS_USB_CDC) && (NRF_SYSTEM_HAS_USB_CDC == 1)
        true,
#else
        false,
#endif
#if defined(NRF_SYSTEM_DEFAULT_SERIAL_USB) && (NRF_SYSTEM_DEFAULT_SERIAL_USB == 1)
        true,
#else
        false,
#endif
#if defined(NRF_SYSTEM_USB_UPLOAD_PREFERRED) && (NRF_SYSTEM_USB_UPLOAD_PREFERRED == 1)
        true,
#else
        false,
#endif
    uploadTouch1200Declared(),
    uploadTouch1200Verified(),
    uf2UploadSupported(),
    uf2UploadNeeded(),
    dfuUtilArgsBoardSpecific(),
#if defined(NRF_SYSTEM_HAS_SWD) && (NRF_SYSTEM_HAS_SWD == 1)
        true,
#else
        false,
#endif
#if defined(NRF_SYSTEM_DEBUG_PADS_ONLY) && (NRF_SYSTEM_DEBUG_PADS_ONLY == 1)
        true,
#else
        false,
#endif
    dfuAltSetting(),
    storageReservedBytes(),
    programFlashBytes(),
    dataRamBytes(),
        detectSerialTopology(),
    detectRuntimeUsbMode(),
    detectBootloaderInterface(),
    detectMonitorTransport(),
        detectUploadTransport(),
    detectUploadTrigger(),
        detectDebugTransport(),
    detectUsbBackend(),
    detectStorageBackend(),
    detectFlashProfile(),
    detectRamProfile(),
    };
    return profile;
}

bool nrfUsbUserPortEnabled() {
    return nrfBoardHasNativeUsb() && nrfSystemProfile().hasUsbCdc;
}

bool nrfUsbServicePortEnabled() {
    if (!nrfBoardHasNativeUsb()) {
        return false;
    }
    const NrfSystemProfile &profile = nrfSystemProfile();
    return profile.prefersUsbUpload || profile.hasUsbCdc || profile.debugTransport == NrfDebugTransport::UsbCdcGdbStub;
}

bool nrfUsbRuntimeEnabled() {
    return nrfUsbServicePortEnabled() || nrfUsbUserPortEnabled();
}

uint8_t nrfBootloaderUploadResetMagic() {
    // Adafruit's bootloader distinguishes between a CDC-only reset target
    // (0x4E) and the full UF2+CDC recovery target (0x57). Host-side upload
    // transport and reset flavor are not always the same thing: on the
    // button-less clone boards we still upload with adafruit-nrfutil over the
    // CDC port, but we want software resets and startup rescue to land in the
    // full UF2 bootloader because that path is the board's most host-visible,
    // recoverable state. NRF_SYSTEM_BOOTLOADER_RESET_MODE lets boards opt into
    // that behavior without lying about the selected upload transport.
    return configuredBootloaderResetMagic();
}

uint8_t nrfBootloaderRescueMagic() {
    return nrfBootloaderUploadResetMagic();
}

void nrfPrepareBootloaderResetRequest(uint8_t marker) {
    reg32(kPowerBase, kPowerGpregret) = marker;
    const uint32_t gpregretReadback = reg32(kPowerBase, kPowerGpregret);
    (void)gpregretReadback;
    if (bootloaderDoubleResetFallbackEnabled()) {
        // Adafruit_nRF52_Bootloader check_dfu_mode() also accepts
        // DFU_DBL_RESET_MAGIC at 0x20007F7C when RESETREAS.RESETPIN is set.
        // Our app currently leaves RESETREAS untouched, so after an initial
        // manual pin-reset this extra breadcrumb gives clone bootloaders a
        // second chance to honor an explicit software upload reset.
        mem32(kDfuDoubleResetMemAddr) = kDfuDoubleResetMagic;
        const uint32_t dblResetReadback = mem32(kDfuDoubleResetMemAddr);
        (void)dblResetReadback;
    }
    syncResetRequestWrites();
}

void nrfPrepareBootloaderResetRequest() {
    nrfPrepareBootloaderResetRequest(configuredBootloaderResetMagic());
}

void nrfClearBootloaderResetRequest() {
    reg32(kPowerBase, kPowerGpregret) = 0UL;
    if (bootloaderDoubleResetFallbackEnabled()) {
        mem32(kDfuDoubleResetMemAddr) = 0UL;
    }
    syncResetRequestWrites();
}

const char *nrfSerialTopologyName(NrfSerialTopology topology) {
    switch (topology) {
        case NrfSerialTopology::NativeUsbCdc:
            return "native-usb-cdc";
        case NrfSerialTopology::UartBridge:
            return "uart-bridge";
        case NrfSerialTopology::UartOnly:
        default:
            return "uart-only";
    }
}

const char *nrfRuntimeUsbModeName(NrfRuntimeUsbMode mode) {
    switch (mode) {
        case NrfRuntimeUsbMode::Cdc:
            return "cdc";
        case NrfRuntimeUsbMode::Disabled:
        default:
            return "disabled";
    }
}

const char *nrfBootloaderInterfaceName(NrfBootloaderInterface interfaceType) {
    switch (interfaceType) {
        case NrfBootloaderInterface::UsbUf2:
            return "usb-uf2";
        case NrfBootloaderInterface::UsbDfu:
            return "usb-dfu";
        case NrfBootloaderInterface::None:
        default:
            return "none";
    }
}

const char *nrfMonitorTransportName(NrfMonitorTransport transport) {
    switch (transport) {
        case NrfMonitorTransport::UsbCdc:
            return "usb-cdc";
        case NrfMonitorTransport::ExternalUart:
            return "external-uart";
        case NrfMonitorTransport::Unavailable:
        default:
            return "unavailable";
    }
}

const char *nrfUploadTransportName(NrfUploadTransport transport) {
    switch (transport) {
        case NrfUploadTransport::UsbBootloader:
            return "usb-bootloader";
        case NrfUploadTransport::UsbBootloaderOrSwd:
            return "usb-bootloader-or-swd";
        case NrfUploadTransport::SwdOpenOcd:
        default:
            return "swd-openocd";
    }
}

const char *nrfUploadTriggerName(NrfUploadTrigger trigger) {
    switch (trigger) {
        case NrfUploadTrigger::Touch1200:
            return "touch-1200bps";
        case NrfUploadTrigger::ExternalProbe:
            return "external-probe";
        case NrfUploadTrigger::None:
        default:
            return "none";
    }
}

const char *nrfDebugTransportName(NrfDebugTransport transport) {
    switch (transport) {
        case NrfDebugTransport::UsbCdcGdbStub:
            return "usb-cdc-gdbstub";
        case NrfDebugTransport::SwdHeader:
            return "swd-header";
        case NrfDebugTransport::SwdPads:
            return "swd-pads";
        case NrfDebugTransport::None:
        default:
            return "none";
    }
}

const char *nrfUsbBackendName(NrfUsbBackend backend) {
    switch (backend) {
        case NrfUsbBackend::Custom:
            return "custom";
        case NrfUsbBackend::Auto:
            return "auto";
        case NrfUsbBackend::Disabled:
        default:
            return "disabled";
    }
}

const char *nrfStorageBackendName(NrfStorageBackend backend) {
    switch (backend) {
        case NrfStorageBackend::LogStructured:
            return "log-structured";
        case NrfStorageBackend::LegacySinglePage:
        default:
            return "legacy-single-page";
    }
}

const char *nrfFlashProfileName(NrfFlashProfile profile) {
    switch (profile) {
        case NrfFlashProfile::InternalFlash512K:
            return "internal-flash-512k";
        case NrfFlashProfile::InternalFlash1M:
            return "internal-flash-1m";
        case NrfFlashProfile::InternalFlash1MWithQspiUnknown:
            return "internal-flash-1m-plus-qspi";
        case NrfFlashProfile::InternalFlash1MWithQspi2M:
            return "internal-flash-1m-plus-qspi-2m";
        case NrfFlashProfile::InternalFlash1MWithQspi8M:
            return "internal-flash-1m-plus-qspi-8m";
        case NrfFlashProfile::Unspecified:
        default:
            return "unspecified";
    }
}

const char *nrfRamProfileName(NrfRamProfile profile) {
    switch (profile) {
        case NrfRamProfile::InternalSram128K:
            return "internal-sram-128k";
        case NrfRamProfile::InternalSram256K:
            return "internal-sram-256k";
        case NrfRamProfile::Unspecified:
        default:
            return "unspecified";
    }
}

const NrfClockProfile &nrfClockProfile() {
    static const NrfClockProfile profile = {
        nrfCpuFrequencyHz(),
        nrfCpuOverclockSupported(),
        nrfCpuOverclockEnabled(),
        "hfclk-64mhz",
        nrfBoardLowFrequencyClockSource(),
        nrfBoardClockSourceEvidenceLevel(),
        nrfBoardLowFrequencyClockSource() != nullptr && strcmp(nrfBoardLowFrequencyClockSource(), "undeclared") != 0,
    };
    return profile;
}
