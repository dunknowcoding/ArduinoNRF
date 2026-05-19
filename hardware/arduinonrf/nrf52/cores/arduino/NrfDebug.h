#pragma once

#include <stdint.h>

enum class NrfDebugProbeKind : uint8_t {
    None,
    CmsisDap,
    JLink,
};

struct NrfDebugConfig {
    bool supported;
    bool ideIntegrationSupported;
    bool usbOnlySourceDebugSupported;
    bool externalProbeRequired;
    bool connectUnderReset;
    uint32_t adapterSpeedKhz;
    NrfDebugProbeKind defaultProbe;
    const char *openOcdScript;
    const char *probeName;
    const char *transportName;

    bool available() const { return supported; }
    bool ideReady() const { return ideIntegrationSupported; }
    bool usbDebugSupported() const { return usbOnlySourceDebugSupported; }
    bool probeRequired() const { return externalProbeRequired; }
};

const NrfDebugConfig &nrfDebugConfig();
const char *nrfDebugProbeName(NrfDebugProbeKind probeKind);