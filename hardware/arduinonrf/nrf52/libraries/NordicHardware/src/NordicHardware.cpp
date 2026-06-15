#include "NordicHardware.h"

#include <NrfClock.h>
#include <NrfBoard.h>
#include <NrfSystem.h>
#include <NrfUsbd.h>
#include <NrfSoftDevice.h>
#include <SPI.h>

namespace {
constexpr uint32_t POWER_BASE = 0x40000000UL;
constexpr uint32_t POWER_RESETREAS = 0x400UL;
constexpr uint32_t TEMP_BASE = 0x4000C000UL;
constexpr uint32_t TEMP_TASKS_START = 0x000UL;
constexpr uint32_t TEMP_TASKS_STOP = 0x004UL;
constexpr uint32_t TEMP_EVENTS_DATARDY = 0x100UL;
constexpr uint32_t TEMP_TEMP = 0x508UL;
constexpr uint32_t RNG_BASE = 0x4000D000UL;
constexpr uint32_t RNG_TASKS_START = 0x000UL;
constexpr uint32_t RNG_TASKS_STOP = 0x004UL;
constexpr uint32_t RNG_EVENTS_VALRDY = 0x100UL;
constexpr uint32_t RNG_VALUE = 0x508UL;
constexpr uint32_t RNG_TIMEOUT_SPINS = 200000UL;
constexpr uint32_t COREDEBUG_DHCSR = 0xE000EDF0UL;
constexpr uint32_t RESETREAS_DOG_MASK = 1UL << 1;
constexpr uint32_t QSPI_BASE = 0x40029000UL;
constexpr uint32_t QSPI_TASKS_ACTIVATE = 0x000UL;
constexpr uint32_t QSPI_TASKS_DEACTIVATE = 0x004UL;
constexpr uint32_t QSPI_TASKS_READSTART = 0x008UL;
constexpr uint32_t QSPI_TASKS_WRITESTART = 0x00CUL;
constexpr uint32_t QSPI_TASKS_ERASESTART = 0x010UL;
constexpr uint32_t QSPI_TASKS_CINSTRSTART = 0x01CUL;
constexpr uint32_t QSPI_EVENTS_READY = 0x100UL;
constexpr uint32_t QSPI_EVENTS_CINSTRDONE = 0x144UL;
constexpr uint32_t QSPI_ENABLE = 0x500UL;
constexpr uint32_t QSPI_IFCONFIG0 = 0x524UL;
constexpr uint32_t QSPI_IFCONFIG1 = 0x528UL;
constexpr uint32_t QSPI_CINSTRCONF = 0x634UL;
constexpr uint32_t QSPI_CINSTRDAT0 = 0x638UL;
constexpr uint32_t QSPI_CINSTRDAT1 = 0x63CUL;
constexpr uint32_t QSPI_PSEL_SCK = 0x640UL;
constexpr uint32_t QSPI_PSEL_CSN = 0x644UL;
constexpr uint32_t QSPI_PSEL_IO0 = 0x648UL;
constexpr uint32_t QSPI_PSEL_IO1 = 0x64CUL;
constexpr uint32_t QSPI_PSEL_IO2 = 0x650UL;
constexpr uint32_t QSPI_PSEL_IO3 = 0x654UL;
constexpr uint32_t QSPI_ENABLE_VALUE = 1UL;
constexpr uint32_t QSPI_TIMEOUT_SPINS = 200000UL;
constexpr uint8_t QSPI_OPCODE_JEDEC_ID = 0x9FUL;
constexpr uint8_t QSPI_OPCODE_DP = 0xB9U;
constexpr uint8_t QSPI_OPCODE_RDP = 0xABU;
constexpr uint32_t CINSTRCONF_LEN_SHIFT = 8U;
constexpr uint32_t CINSTRCONF_WIPWAIT = 1UL << 6;
constexpr uint32_t CINSTRCONF_WREN = 1UL << 7;
constexpr uint32_t CINSTRCONF_LIO2 = 1UL << 20;
constexpr uint32_t CINSTRCONF_LIO3 = 1UL << 21;
constexpr uint32_t CINSTRCONF_LFEN = 1UL << 23;

inline volatile uint32_t &reg32(uint32_t base, uint32_t offset) {
    return *reinterpret_cast<volatile uint32_t *>(base + offset);
}

inline volatile uint32_t &mem32(uint32_t address) {
    return *reinterpret_cast<volatile uint32_t *>(address);
}

bool waitForEvent(uint32_t base, uint32_t offset) {
    for (uint32_t spin = 0; spin < QSPI_TIMEOUT_SPINS; ++spin) {
        if (reg32(base, offset) != 0UL) {
            return true;
        }
    }
    return false;
}

bool qspiPinsValid() {
    return PIN_QSPI_CS != 0xFF && PIN_QSPI_SCK != 0xFF && PIN_QSPI_IO0 != 0xFF &&
           PIN_QSPI_IO1 != 0xFF && PIN_QSPI_IO2 != 0xFF && PIN_QSPI_IO3 != 0xFF;
}
}

NordicHardwareClass NordicHardware;

const char *NordicHardwareClass::chipModel() const {
#if defined(ARDUINO_NRF52_DEVBOARD_833)
    return "nRF52833";
#else
    return "nRF52840";
#endif
}

NrfBoardInfo NordicHardwareClass::boardInfo() const {
    return nrfBoardInfo();
}

NrfBoardSupportStatus NordicHardwareClass::boardSupportStatus() const {
    return nrfBoardSupportStatus();
}

NrfBoardPowerInfo NordicHardwareClass::boardPowerInfo() const {
    return nrfBoardPowerInfo();
}

uint32_t NordicHardwareClass::deviceIdWord(uint8_t index) const {
    return nrfDeviceIdWord(index);
}

float NordicHardwareClass::temperatureC() const {
    reg32(TEMP_BASE, TEMP_EVENTS_DATARDY) = 0UL;
    reg32(TEMP_BASE, TEMP_TASKS_START) = 1UL;
    for (uint32_t spin = 0; spin < RNG_TIMEOUT_SPINS; ++spin) {
        if (reg32(TEMP_BASE, TEMP_EVENTS_DATARDY) != 0UL) {
            break;
        }
    }
    int32_t raw = static_cast<int32_t>(reg32(TEMP_BASE, TEMP_TEMP));
    reg32(TEMP_BASE, TEMP_TASKS_STOP) = 1UL;
    return static_cast<float>(raw) / 4.0f;
}

uint32_t NordicHardwareClass::random32() const {
    uint32_t value = 0;
    reg32(RNG_BASE, RNG_TASKS_START) = 1UL;
    for (uint8_t index = 0; index < 4; ++index) {
        reg32(RNG_BASE, RNG_EVENTS_VALRDY) = 0UL;
        for (uint32_t spin = 0; spin < RNG_TIMEOUT_SPINS; ++spin) {
            if (reg32(RNG_BASE, RNG_EVENTS_VALRDY) != 0UL) {
                break;
            }
        }
        value |= (reg32(RNG_BASE, RNG_VALUE) & 0xFFUL) << (index * 8U);
    }
    reg32(RNG_BASE, RNG_TASKS_STOP) = 1UL;
    return value;
}

void NordicHardwareClass::startLowFrequencyClock() const {
    nrfStartLfclk();
}

void NordicHardwareClass::startHighFrequencyClock() const {
    nrfStartHfclk();
}

bool NordicHardwareClass::lowFrequencyClockRunning() const {
    return nrfLfclkRunning();
}

bool NordicHardwareClass::highFrequencyClockRunning() const {
    return nrfHfclkRunning();
}

uint32_t NordicHardwareClass::resetReason() const {
    return reg32(POWER_BASE, POWER_RESETREAS);
}

void NordicHardwareClass::clearResetReason(uint32_t mask) const {
    reg32(POWER_BASE, POWER_RESETREAS) = mask;
}

bool NordicHardwareClass::debugAttached() const {
    return (mem32(COREDEBUG_DHCSR) & 0x1UL) != 0UL;
}

bool NordicHardwareClass::qspiPresent() const {
    return nrfBoardHasQspiFlash() && qspiPinsValid();
}

bool NordicHardwareClass::qspiBegin() const {
    if (!qspiPresent()) {
        return false;
    }

    reg32(QSPI_BASE, QSPI_PSEL_SCK) = PIN_QSPI_SCK;
    reg32(QSPI_BASE, QSPI_PSEL_CSN) = PIN_QSPI_CS;
    reg32(QSPI_BASE, QSPI_PSEL_IO0) = PIN_QSPI_IO0;
    reg32(QSPI_BASE, QSPI_PSEL_IO1) = PIN_QSPI_IO1;
    reg32(QSPI_BASE, QSPI_PSEL_IO2) = PIN_QSPI_IO2;
    reg32(QSPI_BASE, QSPI_PSEL_IO3) = PIN_QSPI_IO3;
    reg32(QSPI_BASE, QSPI_IFCONFIG0) = 0x00000000UL;
    reg32(QSPI_BASE, QSPI_IFCONFIG1) = 0x00000000UL;
    reg32(QSPI_BASE, QSPI_EVENTS_READY) = 0UL;
    reg32(QSPI_BASE, QSPI_ENABLE) = QSPI_ENABLE_VALUE;
    reg32(QSPI_BASE, QSPI_TASKS_ACTIVATE) = 1UL;
    return waitForEvent(QSPI_BASE, QSPI_EVENTS_READY);
}

uint32_t NordicHardwareClass::qspiJedecId() const {
    if (!qspiBegin()) {
        return 0UL;
    }

    reg32(QSPI_BASE, QSPI_EVENTS_CINSTRDONE) = 0UL;
    reg32(QSPI_BASE, QSPI_CINSTRDAT0) = 0UL;
    reg32(QSPI_BASE, QSPI_CINSTRDAT1) = 0UL;
    reg32(QSPI_BASE, QSPI_CINSTRCONF) =
        static_cast<uint32_t>(QSPI_OPCODE_JEDEC_ID) |
        (4UL << CINSTRCONF_LEN_SHIFT) |
        CINSTRCONF_LFEN;
    reg32(QSPI_BASE, QSPI_TASKS_CINSTRSTART) = 1UL;
    if (!waitForEvent(QSPI_BASE, QSPI_EVENTS_CINSTRDONE)) {
        return 0UL;
    }
    return reg32(QSPI_BASE, QSPI_CINSTRDAT0) & 0x00FFFFFFUL;
}

void NordicHardwareClass::qspiDeepPowerDown() const {
    if (!qspiBegin()) {
        return;
    }
    reg32(QSPI_BASE, QSPI_EVENTS_CINSTRDONE) = 0UL;
    reg32(QSPI_BASE, QSPI_CINSTRCONF) = static_cast<uint32_t>(QSPI_OPCODE_DP) | (1UL << CINSTRCONF_LEN_SHIFT) | CINSTRCONF_LFEN;
    reg32(QSPI_BASE, QSPI_TASKS_CINSTRSTART) = 1UL;
    waitForEvent(QSPI_BASE, QSPI_EVENTS_CINSTRDONE);
}

void NordicHardwareClass::qspiWake() const {
    if (!qspiBegin()) {
        return;
    }
    reg32(QSPI_BASE, QSPI_EVENTS_CINSTRDONE) = 0UL;
    reg32(QSPI_BASE, QSPI_CINSTRCONF) = static_cast<uint32_t>(QSPI_OPCODE_RDP) | (1UL << CINSTRCONF_LEN_SHIFT) | CINSTRCONF_LFEN;
    reg32(QSPI_BASE, QSPI_TASKS_CINSTRSTART) = 1UL;
    waitForEvent(QSPI_BASE, QSPI_EVENTS_CINSTRDONE);
}

bool NordicHardwareClass::usbReady() const {
    if (nrfBoardHasNativeUsb()) {
        return nrfUsbdDriver().ready();
    }
    return false;
}

NordicPwmCapabilities NordicHardwareClass::pwmCapabilities() const {
    return {
        nrfPwmChannelCapacity(),
        nrfPwmActiveChannels(),
        nrfPwmChannelsIndependent(),
        nrfPwmSharedTimer(),
        nrfPwmIndependentTimersSupported(),
        nrfPwmTimerGroupCount(),
        nrfPwmPolarityConfigurable(),
        nrfPwmCenterAlignedSupported(),
        nrfPwmCounterClockHz(),
        nrfPwmFrequencyHz(),
        nrfPwmNativeResolutionBits(),
        nrfPwmConfiguredResolutionBits(),
    };
}

NordicTimerCapabilities NordicHardwareClass::timerCapabilities() const {
    return {
        nrfToneSupported(),
        nrfServoSupported(),
    };
}

NordicAnalogCapabilities NordicHardwareClass::analogCapabilities() const {
    return {
        nrfAdcPresent(),
        nrfAdcChannelCount(),
        nrfAdcNativeResolutionBits(),
        nrfAdcConfiguredResolutionBits(),
        nrfAdcReferenceConfigurable(),
        nrfAdcReference(),
        nrfAdcGainConfigurable(),
        nrfAdcGain(),
        nrfAdcAcquisitionTimeConfigurable(),
        nrfAdcAcquisitionTimeUs(),
        nrfAdcCalibrationSupported(),
        nrfAdcCalibrationPending(),
        nrfDacPresent(),
        nrfDacChannelCount(),
    };
}

NordicSpiCapabilities NordicHardwareClass::spiCapabilities() const {
    return {
        true,
        SPI.configuredClockHz(),
        SPI.maxClockHz(),
        SPI.maxClockHz() >= 8000000UL,
    };
}

NordicClockCapabilities NordicHardwareClass::clockCapabilities() const {
    const NrfClockProfile &profile = nrfClockProfile();
    return {
        profile.cpuFrequencyHz,
        profile.overclockSupported,
        profile.overclockEnabled,
        lowFrequencyClockRunning(),
        highFrequencyClockRunning(),
        profile.cpuClockSource,
        profile.lowFrequencyClockSource,
        profile.clockSourceEvidenceLevel,
        profile.lowFrequencyClockDeclared,
    };
}

NrfPinInfo NordicHardwareClass::pinInfo(uint8_t pin) const {
    return nrfPinInfo(pin);
}

bool NordicHardwareClass::batterySenseAvailable() const {
    return nrfBatteryReadingSupported();
}

int NordicHardwareClass::batteryRaw() const {
    return nrfBatteryRaw();
}

uint32_t NordicHardwareClass::batteryMillivolts() const {
    return nrfBatteryMillivolts();
}

bool NordicHardwareClass::adcPinSupported(uint8_t pin) const {
    return nrfBoardPinSupportsAnalogInput(pin);
}

bool NordicHardwareClass::dacPresent() const {
    return nrfDacPresent();
}

bool NordicHardwareClass::spiSupportsFrequency(uint32_t hz) const {
    return SPI.supportsClockHz(hz);
}

bool NordicHardwareClass::runSelfTest(NordicSelfTestReport &report) const {
    startLowFrequencyClock();
    startHighFrequencyClock();
    report.lfclkOk = lowFrequencyClockRunning();
    report.hfclkOk = highFrequencyClockRunning();
    report.resetReason = resetReason();
    report.watchdogReset = (report.resetReason & RESETREAS_DOG_MASK) != 0UL;
    report.debugAttached = debugAttached();
    report.deviceIdOk = deviceIdWord(0) != 0UL || deviceIdWord(1) != 0UL;
    report.qspiOk = !qspiPresent() || qspiJedecId() != 0UL;
    report.usbOk = !nrfBoardHasNativeUsb() || usbReady();

    const float temperature = temperatureC();
    report.temperatureOk = temperature > -80.0f && temperature < 150.0f;
    report.rngOk = random32() != 0UL;

    return report.lfclkOk && report.hfclkOk && report.deviceIdOk && report.temperatureOk && report.rngOk && report.qspiOk && report.usbOk;
}

NordicSoftDeviceInfo NordicHardwareClass::softDeviceInfo() const {
    NordicSoftDeviceInfo info;
    info.present = NrfSoftDevice::isPresent();
    info.enabled = (NrfSoftDevice::status() == NrfSoftDevice::Status::Enabled);
    info.baseAddress = NrfSoftDevice::baseAddress();
    info.appStartAddress = NrfSoftDevice::appStartAddress();
    info.firmwareId = NrfSoftDevice::firmwareId();
    info.versionRaw = NrfSoftDevice::versionRaw();
    return info;
}

bool NordicHardwareClass::softDevicePresent() const {
    return NrfSoftDevice::isPresent();
}
