#pragma once

#include <Arduino.h>

#include <stdbool.h>
#include <stdint.h>

struct NordicSelfTestReport {
    bool lfclkOk;
    bool hfclkOk;
    bool rngOk;
    bool temperatureOk;
    bool deviceIdOk;
    bool qspiOk;
    bool usbOk;
    bool debugAttached;
    bool watchdogReset;
    uint32_t resetReason;
};

struct NordicPwmCapabilities {
    uint8_t channelCapacity;
    uint8_t activeChannels;
    bool channelsIndependent;
    bool sharedTimer;
    bool independentTimersSupported;
    uint8_t timerGroupCount;
    bool polarityConfigurable;
    bool centerAlignedSupported;
    uint32_t counterClockHz;
    uint32_t frequencyHz;
    uint8_t nativeResolutionBits;
    uint8_t configuredResolutionBits;
};

struct NordicTimerCapabilities {
    bool toneSupported;
    bool servoSupported;
};

struct NordicAnalogCapabilities {
    bool adcPresent;
    uint8_t adcChannelCount;
    uint8_t adcNativeResolutionBits;
    uint8_t adcConfiguredResolutionBits;
    bool referenceConfigurable;
    uint8_t reference;
    bool gainConfigurable;
    NrfAdcGain gain;
    bool acquisitionTimeConfigurable;
    uint8_t acquisitionTimeUs;
    bool calibrationSupported;
    bool calibrationPending;
    bool dacPresent;
    uint8_t dacChannelCount;
};

struct NordicSpiCapabilities {
    bool supported;
    uint32_t configuredFrequencyHz;
    uint32_t maxFrequencyHz;
    bool highSpeedCapable;
};

struct NordicClockCapabilities {
    uint32_t cpuFrequencyHz;
    bool overclockSupported;
    bool overclockEnabled;
    bool lfclkRunning;
    bool hfclkRunning;
    const char *cpuClockSource;
    const char *lowFrequencyClockSource;
    const char *clockSourceEvidenceLevel;
    bool lowFrequencyClockDeclared;
};

// SoftDevice / MBR awareness.  This core is deliberately SoftDevice-free (the
// radio is owned bare-metal by NimBLE/Thread, or by an external CC2530 for
// Zigbee), so a SoftDevice - when one is present in flash, as on the nice!nano
// S140 - stays DORMANT and never claims a peripheral.  These fields report what
// the core detected at boot.  See docs/platform/SOFTDEVICE.md.
struct NordicSoftDeviceInfo {
    bool present;             // an MBR + SoftDevice image is in flash
    bool enabled;             // a SoftDevice was started via the opt-in hook (false on bare-metal)
    uint32_t baseAddress;     // SoftDevice base (= MBR size, 0x1000) when present
    uint32_t appStartAddress; // where the application is linked (0x1000 with no SoftDevice)
    uint32_t firmwareId;      // SD_FWID low 16 bits (0 when absent)
    uint32_t versionRaw;      // SD_VERSION: major*1e6 + minor*1e3 + revision (0 when absent)
};

class NordicHardwareClass {
public:
    const char *chipModel() const;
    NrfBoardInfo boardInfo() const;
    NrfBoardSupportStatus boardSupportStatus() const;
    NrfBoardPowerInfo boardPowerInfo() const;
    uint32_t deviceIdWord(uint8_t index) const;
    float temperatureC() const;
    uint32_t random32() const;
    void startLowFrequencyClock() const;
    void startHighFrequencyClock() const;
    bool lowFrequencyClockRunning() const;
    bool highFrequencyClockRunning() const;
    uint32_t resetReason() const;
    void clearResetReason(uint32_t mask = 0xFFFFFFFFUL) const;
    bool debugAttached() const;
    bool qspiPresent() const;
    bool qspiBegin() const;
    uint32_t qspiJedecId() const;
    void qspiDeepPowerDown() const;
    void qspiWake() const;
    bool usbReady() const;
    NordicPwmCapabilities pwmCapabilities() const;
    NordicTimerCapabilities timerCapabilities() const;
    NordicAnalogCapabilities analogCapabilities() const;
    NordicSpiCapabilities spiCapabilities() const;
    NordicClockCapabilities clockCapabilities() const;
    NrfPinInfo pinInfo(uint8_t pin) const;
    bool batterySenseAvailable() const;
    int batteryRaw() const;
    uint32_t batteryMillivolts() const;
    bool adcPinSupported(uint8_t pin) const;
    bool dacPresent() const;
    bool spiSupportsFrequency(uint32_t hz) const;
    bool runSelfTest(NordicSelfTestReport &report) const;

    // SoftDevice / MBR awareness (see NordicSoftDeviceInfo).
    NordicSoftDeviceInfo softDeviceInfo() const;
    bool softDevicePresent() const;
};

extern NordicHardwareClass NordicHardware;
