#include "NrfDebug.h"

#include "NrfSystem.h"

namespace {
bool usbGdbStubEnabled() {
    return nrfSystemProfile().debugTransport == NrfDebugTransport::UsbCdcGdbStub;
}

uint32_t openOcdResetDelayMs() {
    if (usbGdbStubEnabled()) {
        return 0U;
    }
#if defined(ARDUINO_NRF52_USB_DONGLE)
    return 400U;
#else
    return 1000U;
#endif
}

const char *defaultOpenOcdConfig() {
    if (usbGdbStubEnabled()) {
        return "external-gdb-target";
    }
    return "nrf52-cmsis-dap.cfg";
}

NrfDebugProbeKind defaultProbeKind() {
    if (usbGdbStubEnabled()) {
        return NrfDebugProbeKind::None;
    }

    return NrfDebugProbeKind::CmsisDap;
}
}

const NrfDebugConfig &nrfDebugConfig() {
    static const NrfDebugConfig config = {
        nrfSystemProfile().hasSwdDebug || usbGdbStubEnabled(),
        nrfSystemProfile().hasSwdDebug || usbGdbStubEnabled(),
        usbGdbStubEnabled(),
        !usbGdbStubEnabled() && nrfSystemProfile().hasSwdDebug,
        !usbGdbStubEnabled(),
        openOcdResetDelayMs(),
        defaultProbeKind(),
        defaultOpenOcdConfig(),
        nrfSystemProfile().defaultDebugProbe,
        nrfDebugTransportName(nrfSystemProfile().debugTransport),
    };
    return config;
}

const char *nrfDebugProbeName(NrfDebugProbeKind probeKind) {
    switch (probeKind) {
        case NrfDebugProbeKind::CmsisDap:
            return "CMSIS-DAP";
        case NrfDebugProbeKind::JLink:
            return "J-Link";
        case NrfDebugProbeKind::None:
        default:
            return "none";
    }
}