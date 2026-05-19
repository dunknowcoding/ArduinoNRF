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
};

extern NordicHardwareClass NordicHardware;
