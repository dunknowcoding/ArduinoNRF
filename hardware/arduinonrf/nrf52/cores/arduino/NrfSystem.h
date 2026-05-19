#pragma once

#include <stdint.h>

enum class NrfSerialTopology : uint8_t {
    NativeUsbCdc,
    UartBridge,
    UartOnly,
};

enum class NrfRuntimeUsbMode : uint8_t {
    Disabled,
    Cdc,
};

enum class NrfBootloaderInterface : uint8_t {
    None,
    UsbDfu,
    UsbUf2,
};

enum class NrfMonitorTransport : uint8_t {
    Unavailable,
    UsbCdc,
    ExternalUart,
};

enum class NrfUploadTransport : uint8_t {
    SwdOpenOcd,
    UsbBootloader,
    UsbBootloaderOrSwd,
};

enum class NrfUploadTrigger : uint8_t {
    None,
    Touch1200,
    ExternalProbe,
};

enum class NrfDebugTransport : uint8_t {
    None,
    SwdHeader,
    SwdPads,
    UsbCdcGdbStub,
};

enum class NrfUsbBackend : uint8_t {
    Disabled,
    Custom,
    Auto,
};

enum class NrfStorageBackend : uint8_t {
    LegacySinglePage,
    LogStructured,
};

enum class NrfFlashProfile : uint8_t {
    Unspecified,
    InternalFlash512K,
    InternalFlash1M,
    InternalFlash1MWithQspiUnknown,
    InternalFlash1MWithQspi2M,
    InternalFlash1MWithQspi8M,
};

enum class NrfRamProfile : uint8_t {
    Unspecified,
    InternalSram128K,
    InternalSram256K,
};

struct NrfSystemProfile {
    const char *boardName;
    const char *usbProduct;
    const char *defaultDebugProbe;
    uint16_t usbVid;
    uint16_t usbPid;
    uint8_t serialRxPin;
    uint8_t serialTxPin;
    bool hasUsbCdc;
    bool serialMonitorOnUsb;
    bool prefersUsbUpload;
    bool uploadTouch1200Declared;
    bool uploadTouch1200Verified;
    bool uf2UploadSupported;
    bool uf2UploadNeeded;
    bool dfuUtilArgsBoardSpecific;
    bool hasSwdDebug;
    bool swdPadsOnly;
    uint8_t dfuAltSetting;
    uint32_t storageReservedBytes;
    uint32_t programFlashBytes;
    uint32_t dataRamBytes;
    NrfSerialTopology serialTopology;
    NrfRuntimeUsbMode runtimeUsbMode;
    NrfBootloaderInterface bootloaderInterface;
    NrfMonitorTransport monitorTransport;
    NrfUploadTransport uploadTransport;
    NrfUploadTrigger uploadTrigger;
    NrfDebugTransport debugTransport;
    NrfUsbBackend usbBackend;
    NrfStorageBackend storageBackend;
    NrfFlashProfile flashProfile;
    NrfRamProfile ramProfile;
};

struct NrfClockProfile {
    uint32_t cpuFrequencyHz;
    bool overclockSupported;
    bool overclockEnabled;
    const char *cpuClockSource;
    const char *lowFrequencyClockSource;
    const char *clockSourceEvidenceLevel;
    bool lowFrequencyClockDeclared;
};

const NrfSystemProfile &nrfSystemProfile();
const NrfClockProfile &nrfClockProfile();
bool nrfUsbUserPortEnabled();
bool nrfUsbServicePortEnabled();
bool nrfUsbRuntimeEnabled();
uint8_t nrfBootloaderUploadResetMagic();
uint8_t nrfBootloaderRescueMagic();
void nrfPrepareBootloaderResetRequest(uint8_t marker);
void nrfPrepareBootloaderResetRequest();
void nrfClearBootloaderResetRequest();
const char *nrfSerialTopologyName(NrfSerialTopology topology);
const char *nrfRuntimeUsbModeName(NrfRuntimeUsbMode mode);
const char *nrfBootloaderInterfaceName(NrfBootloaderInterface interfaceType);
const char *nrfMonitorTransportName(NrfMonitorTransport transport);
const char *nrfUploadTransportName(NrfUploadTransport transport);
const char *nrfUploadTriggerName(NrfUploadTrigger trigger);
const char *nrfDebugTransportName(NrfDebugTransport transport);
const char *nrfUsbBackendName(NrfUsbBackend backend);
const char *nrfStorageBackendName(NrfStorageBackend backend);
const char *nrfFlashProfileName(NrfFlashProfile profile);
const char *nrfRamProfileName(NrfRamProfile profile);
