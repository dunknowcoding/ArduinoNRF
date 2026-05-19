#include "Arduino.h"

#include "NrfGdbStub.h"
#include "NrfUsbSerial.h"
#include "USBDevice.h"

namespace {
volatile unsigned long g_millis = 0;
uint8_t g_pinModes[64] = {0};
uint8_t g_pinValues[64] = {0};
uint16_t g_pwmValues[64] = {0};
uint8_t g_pwmSlots[64] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};
uint8_t g_pwmPins[4] = {0xFF, 0xFF, 0xFF, 0xFF};
voidFuncPtr g_interruptHandlers[64] = {nullptr};
uint8_t g_interruptModes[64] = {0};
uint8_t g_interruptChannels[64] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};
uint8_t g_gpiotePins[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

constexpr uint32_t SYSTICK_BASE = 0xE000E010UL;
constexpr uint32_t POWER_BASE = 0x40000000UL;
constexpr uint32_t POWER_SYSTEMOFF = 0x500UL;
constexpr uint32_t NVIC_ISER_BASE = 0xE000E100UL;
constexpr uint32_t NVIC_ICER_BASE = 0xE000E180UL;
constexpr uint32_t GPIO_PORT0_BASE = 0x50000000UL;
constexpr uint32_t GPIO_PORT_STRIDE = 0x300UL;
constexpr uint32_t GPIO_INPUT_CONNECT = 0x0UL;
constexpr uint32_t GPIO_INPUT_DISCONNECT = 0x2UL;
constexpr uint32_t GPIO_PULL_DISABLED = 0x0UL;
constexpr uint32_t GPIO_PULL_DOWN = 0x1UL;
constexpr uint32_t GPIO_PULL_UP = 0x3UL;
constexpr uint32_t GPIO_SENSE_DISABLED = 0x0UL;
constexpr uint32_t GPIOTE_BASE = 0x40006000UL;
constexpr uint32_t GPIOTE_EVENTS_IN_BASE = 0x100UL;
constexpr uint32_t GPIOTE_INTENSET = 0x304UL;
constexpr uint32_t GPIOTE_INTENCLR = 0x308UL;
constexpr uint32_t GPIOTE_CONFIG_BASE = 0x510UL;
constexpr uint32_t GPIOTE_IRQ_NUMBER = 6UL;
constexpr uint32_t GPIOTE_MODE_EVENT = 1UL;
constexpr uint32_t GPIOTE_POLARITY_LOTOHI = 1UL;
constexpr uint32_t GPIOTE_POLARITY_HITOLO = 2UL;
constexpr uint32_t GPIOTE_POLARITY_TOGGLE = 3UL;
constexpr uint32_t SAADC_BASE = 0x40007000UL;
constexpr uint32_t SAADC_TASKS_START = 0x000UL;
constexpr uint32_t SAADC_TASKS_SAMPLE = 0x004UL;
constexpr uint32_t SAADC_TASKS_STOP = 0x008UL;
constexpr uint32_t SAADC_TASKS_CALIBRATEOFFSET = 0x00CUL;
constexpr uint32_t SAADC_EVENTS_STARTED = 0x100UL;
constexpr uint32_t SAADC_EVENTS_END = 0x104UL;
constexpr uint32_t SAADC_EVENTS_CALIBRATEDONE = 0x110UL;
constexpr uint32_t SAADC_EVENTS_STOPPED = 0x108UL;
constexpr uint32_t SAADC_ENABLE = 0x500UL;
constexpr uint32_t SAADC_CH0_PSELP = 0x510UL;
constexpr uint32_t SAADC_CH0_PSELN = 0x514UL;
constexpr uint32_t SAADC_CH0_CONFIG = 0x518UL;
constexpr uint32_t SAADC_RESULT_PTR = 0x62CUL;
constexpr uint32_t SAADC_RESULT_MAXCNT = 0x630UL;
constexpr uint32_t SAADC_RESOLUTION = 0x5F0UL;
constexpr uint32_t SAADC_SAMPLERATE = 0x5F8UL;
constexpr uint32_t SAADC_ENABLE_DISABLED = 0UL;
constexpr uint32_t SAADC_ENABLE_ENABLED = 1UL;
constexpr uint32_t SAADC_CH_PSEL_NC = 0UL;
constexpr uint32_t SAADC_CH_PSEL_VDD = 9UL;
constexpr uint32_t SAADC_CH_PSEL_VDDHDIV5 = 0x0DUL;
constexpr uint32_t SAADC_CH_CONFIG_GAIN_SHIFT = 8UL;
constexpr uint32_t SAADC_CH_CONFIG_REFSEL_SHIFT = 12UL;
constexpr uint32_t SAADC_CH_CONFIG_TACQ_SHIFT = 16UL;
constexpr uint32_t SAADC_TIMEOUT_SPINS = 200000UL;
constexpr uint32_t PWM0_BASE = 0x4001C000UL;
constexpr uint32_t PWM_TASKS_STOP = 0x004UL;
constexpr uint32_t PWM_TASKS_SEQSTART0 = 0x008UL;
constexpr uint32_t PWM_ENABLE = 0x500UL;
constexpr uint32_t PWM_MODE = 0x504UL;
constexpr uint32_t PWM_COUNTERTOP = 0x508UL;
constexpr uint32_t PWM_PRESCALER = 0x50CUL;
constexpr uint32_t PWM_DECODER = 0x510UL;
constexpr uint32_t PWM_LOOP = 0x514UL;
constexpr uint32_t PWM_SEQ0_PTR = 0x520UL;
constexpr uint32_t PWM_SEQ0_CNT = 0x524UL;
constexpr uint32_t PWM_SEQ0_REFRESH = 0x528UL;
constexpr uint32_t PWM_SEQ0_ENDDELAY = 0x52CUL;
constexpr uint32_t PWM_PSEL_OUT0 = 0x560UL;
constexpr uint32_t PWM_PSEL_OUT1 = 0x564UL;
constexpr uint32_t PWM_PSEL_OUT2 = 0x568UL;
constexpr uint32_t PWM_PSEL_OUT3 = 0x56CUL;
constexpr uint32_t PWM_ENABLE_DISABLED = 0UL;
constexpr uint32_t PWM_ENABLE_ENABLED = 1UL;
constexpr uint32_t PWM_POLARITY_INVERTED = 0x8000U;
constexpr uint32_t PWM_DECODER_LOAD_INDIVIDUAL = 2UL;
constexpr uint32_t PWM_PSEL_DISCONNECTED = 0xFFFFFFFFUL;
constexpr uint16_t PWM_COUNTERTOP_MIN = 3U;
constexpr uint16_t PWM_COUNTERTOP_MAX = 32767U;
constexpr uint8_t PWM_PRESCALER_MAX = 7U;
constexpr uint32_t PWM_BASE_CLOCK_HZ = 16000000UL;

uint16_t g_pwmSequence[4] = {0, 0, 0, 0};
uint8_t g_analogReadResolutionBits = 10;
uint8_t g_analogWriteResolutionBits = 10;
uint8_t g_analogReferenceMode = INTERNAL;
NrfAdcGain g_analogGain = NRF_ADC_GAIN_1_6;
uint8_t g_analogAcquisitionTimeUs = 10U;
uint8_t g_lastCalibratedReferenceMode = 0xFFU;
NrfAdcGain g_lastCalibratedGain = NRF_ADC_GAIN_1_6;
uint8_t g_lastCalibratedAcquisitionTimeUs = 0U;
uint32_t g_randomState = 0x6D2B79F5UL;
uint8_t g_pwmPrescaler = 4U;
uint16_t g_pwmCounterTop = 1023U;
NrfPwmWriteStatus g_pwmLastWriteStatus = NRF_PWM_WRITE_OK;

uint8_t adcHardwareResolutionBits();
uint32_t saadcResolutionRegisterValue(uint8_t bits);
int scaleFromBitDepth(int value, uint8_t fromBits, uint8_t toBits);

struct SysTickRegisters {
    volatile uint32_t CTRL;
    volatile uint32_t LOAD;
    volatile uint32_t VAL;
    volatile uint32_t CALIB;
};

struct GpioRegisters {
    volatile uint32_t RESERVED0[321];
    volatile uint32_t OUT;
    volatile uint32_t OUTSET;
    volatile uint32_t OUTCLR;
    volatile uint32_t IN;
    volatile uint32_t DIR;
    volatile uint32_t DIRSET;
    volatile uint32_t DIRCLR;
    volatile uint32_t LATCH;
    volatile uint32_t DETECTMODE;
    volatile uint32_t RESERVED1[118];
    volatile uint32_t PIN_CNF[32];
};

inline SysTickRegisters &systick() {
    return *reinterpret_cast<SysTickRegisters *>(SYSTICK_BASE);
}

inline GpioRegisters &gpioPort(uint32_t portIndex) {
    return *reinterpret_cast<GpioRegisters *>(GPIO_PORT0_BASE + (portIndex * GPIO_PORT_STRIDE));
}

inline volatile uint32_t &reg32(uint32_t base, uint32_t offset) {
    return *reinterpret_cast<volatile uint32_t *>(base + offset);
}

inline uint32_t rawPinFor(uint8_t pin) {
    if (pin >= PINS_COUNT) {
        return NOT_A_PIN;
    }
    return g_ADigitalPinMap[pin];
}

inline bool isValidRawPin(uint32_t rawPin) {
    return rawPin != NOT_A_PIN && rawPin < 64U;
}

inline uint32_t portIndexFor(uint32_t rawPin) {
    return rawPin >> 5;
}

inline uint32_t bitIndexFor(uint32_t rawPin) {
    return rawPin & 0x1FU;
}

inline uint32_t pinConfigurationFor(uint8_t mode) {
    switch (mode) {
        case OUTPUT:
            return (1UL << 0) | (GPIO_INPUT_DISCONNECT << 1) | (GPIO_PULL_DISABLED << 2);
        case INPUT_PULLUP:
            return (0UL << 0) | (GPIO_INPUT_CONNECT << 1) | (GPIO_PULL_UP << 2) | (GPIO_SENSE_DISABLED << 16);
        case INPUT_PULLDOWN:
            return (0UL << 0) | (GPIO_INPUT_CONNECT << 1) | (GPIO_PULL_DOWN << 2) | (GPIO_SENSE_DISABLED << 16);
        case INPUT:
        default:
            return (0UL << 0) | (GPIO_INPUT_CONNECT << 1) | (GPIO_PULL_DISABLED << 2) | (GPIO_SENSE_DISABLED << 16);
    }
}

inline uint32_t systickReload(void) {
    return (F_CPU / 1000UL) - 1UL;
}

inline bool isAnalogCapableRawPin(uint32_t rawPin) {
    switch (rawPin) {
        case 2:
        case 3:
        case 4:
        case 5:
        case 28:
        case 29:
        case 30:
        case 31:
            return true;
        default:
            return false;
    }
}

inline uint32_t saadcPselForRawPin(uint32_t rawPin) {
    switch (rawPin) {
        case 2: return 1UL;
        case 3: return 2UL;
        case 4: return 3UL;
        case 5: return 4UL;
        case 28: return 5UL;
        case 29: return 6UL;
        case 30: return 7UL;
        case 31: return 8UL;
        default: return 0UL;
    }
}

bool saadcReferenceSupported(uint8_t reference) {
    return reference == DEFAULT || reference == INTERNAL || reference == AR_VDD4;
}

uint8_t normalizeSaadcReference(uint8_t reference) {
    if (reference == DEFAULT) {
        return INTERNAL;
    }
    return reference;
}

bool saadcGainSupported(NrfAdcGain gain) {
    return gain >= NRF_ADC_GAIN_1_6 && gain <= NRF_ADC_GAIN_4;
}

bool saadcAcquisitionTimeSupported(uint8_t microseconds) {
    switch (microseconds) {
        case 3U:
        case 5U:
        case 10U:
        case 15U:
        case 20U:
        case 40U:
            return true;
        default:
            return false;
    }
}

uint32_t saadcReferenceRegisterValue(uint8_t reference) {
    if (normalizeSaadcReference(reference) == AR_VDD4) {
        return (1UL << SAADC_CH_CONFIG_REFSEL_SHIFT);
    }
    return 0UL;
}

uint32_t saadcGainRegisterValue(NrfAdcGain gain) {
    return static_cast<uint32_t>(gain) << SAADC_CH_CONFIG_GAIN_SHIFT;
}

uint32_t saadcAcquisitionTimeRegisterValue(uint8_t microseconds) {
    switch (microseconds) {
        case 3U:
            return 0UL << SAADC_CH_CONFIG_TACQ_SHIFT;
        case 5U:
            return 1UL << SAADC_CH_CONFIG_TACQ_SHIFT;
        case 10U:
            return 2UL << SAADC_CH_CONFIG_TACQ_SHIFT;
        case 15U:
            return 3UL << SAADC_CH_CONFIG_TACQ_SHIFT;
        case 20U:
            return 4UL << SAADC_CH_CONFIG_TACQ_SHIFT;
        case 40U:
            return 5UL << SAADC_CH_CONFIG_TACQ_SHIFT;
        default:
            return 2UL << SAADC_CH_CONFIG_TACQ_SHIFT;
    }
}

uint32_t saadcConfigValue(uint8_t reference, NrfAdcGain gain, uint8_t microseconds) {
    return saadcGainRegisterValue(gain) |
           saadcReferenceRegisterValue(reference) |
           saadcAcquisitionTimeRegisterValue(microseconds);
}

bool saadcConfigNeedsCalibration(uint8_t reference, NrfAdcGain gain, uint8_t microseconds) {
    return g_lastCalibratedReferenceMode != normalizeSaadcReference(reference) ||
           g_lastCalibratedGain != gain ||
           g_lastCalibratedAcquisitionTimeUs != microseconds;
}

bool waitForSaadcEvent(uint32_t offset) {
    for (uint32_t spin = 0; spin < SAADC_TIMEOUT_SPINS; ++spin) {
        if (reg32(SAADC_BASE, offset) != 0UL) {
            reg32(SAADC_BASE, offset) = 0UL;
            return true;
        }
    }
    reg32(SAADC_BASE, offset) = 0UL;
    return false;
}

bool calibrateSaadc(uint8_t reference, NrfAdcGain gain, uint8_t microseconds) {
    reg32(SAADC_BASE, SAADC_ENABLE) = SAADC_ENABLE_ENABLED;
    reg32(SAADC_BASE, SAADC_CH0_PSELN) = SAADC_CH_PSEL_NC;
    reg32(SAADC_BASE, SAADC_CH0_PSELP) = SAADC_CH_PSEL_NC;
    reg32(SAADC_BASE, SAADC_CH0_CONFIG) = saadcConfigValue(reference, gain, microseconds);
    reg32(SAADC_BASE, SAADC_EVENTS_CALIBRATEDONE) = 0UL;
    reg32(SAADC_BASE, SAADC_TASKS_CALIBRATEOFFSET) = 1UL;
    const bool calibrated = waitForSaadcEvent(SAADC_EVENTS_CALIBRATEDONE);
    reg32(SAADC_BASE, SAADC_ENABLE) = SAADC_ENABLE_DISABLED;

    if (calibrated) {
        g_lastCalibratedReferenceMode = normalizeSaadcReference(reference);
        g_lastCalibratedGain = gain;
        g_lastCalibratedAcquisitionTimeUs = microseconds;
    }

    return calibrated;
}

bool ensureSaadcCalibrated(uint8_t reference, NrfAdcGain gain, uint8_t microseconds) {
    if (!saadcConfigNeedsCalibration(reference, gain, microseconds)) {
        return true;
    }
    return calibrateSaadc(reference, gain, microseconds);
}

uint32_t adcFullScaleMillivolts(uint8_t reference, NrfAdcGain gain) {
    if (normalizeSaadcReference(reference) == AR_VDD4) {
        return 0UL;
    }

    uint32_t numerator = 1UL;
    uint32_t denominator = 1UL;
    switch (gain) {
        case NRF_ADC_GAIN_1_6:
            numerator = 1UL;
            denominator = 6UL;
            break;
        case NRF_ADC_GAIN_1_5:
            numerator = 1UL;
            denominator = 5UL;
            break;
        case NRF_ADC_GAIN_1_4:
            numerator = 1UL;
            denominator = 4UL;
            break;
        case NRF_ADC_GAIN_1_3:
            numerator = 1UL;
            denominator = 3UL;
            break;
        case NRF_ADC_GAIN_1_2:
            numerator = 1UL;
            denominator = 2UL;
            break;
        case NRF_ADC_GAIN_1:
            numerator = 1UL;
            denominator = 1UL;
            break;
        case NRF_ADC_GAIN_2:
            numerator = 2UL;
            denominator = 1UL;
            break;
        case NRF_ADC_GAIN_4:
            numerator = 4UL;
            denominator = 1UL;
            break;
    }

    return (600UL * denominator) / numerator;
}

int analogReadFromPselWithConfig(uint32_t psel, uint8_t reference, NrfAdcGain gain, uint8_t microseconds) {
    if (psel == SAADC_CH_PSEL_NC) {
        return 0;
    }

    if (!ensureSaadcCalibrated(reference, gain, microseconds)) {
        return 0;
    }

    int16_t sample = 0;
    const uint8_t hardwareBits = adcHardwareResolutionBits();

    reg32(SAADC_BASE, SAADC_ENABLE) = SAADC_ENABLE_ENABLED;
    reg32(SAADC_BASE, SAADC_RESOLUTION) = saadcResolutionRegisterValue(hardwareBits);
    reg32(SAADC_BASE, SAADC_SAMPLERATE) = 0UL;
    reg32(SAADC_BASE, SAADC_CH0_PSELN) = SAADC_CH_PSEL_NC;
    reg32(SAADC_BASE, SAADC_CH0_PSELP) = psel;
    reg32(SAADC_BASE, SAADC_CH0_CONFIG) = saadcConfigValue(reference, gain, microseconds);
    reg32(SAADC_BASE, SAADC_RESULT_PTR) = reinterpret_cast<uint32_t>(&sample);
    reg32(SAADC_BASE, SAADC_RESULT_MAXCNT) = 1UL;
    reg32(SAADC_BASE, SAADC_EVENTS_STARTED) = 0UL;
    reg32(SAADC_BASE, SAADC_EVENTS_END) = 0UL;
    reg32(SAADC_BASE, SAADC_EVENTS_STOPPED) = 0UL;
    reg32(SAADC_BASE, SAADC_TASKS_START) = 1UL;

    if (!waitForSaadcEvent(SAADC_EVENTS_STARTED)) {
        reg32(SAADC_BASE, SAADC_ENABLE) = SAADC_ENABLE_DISABLED;
        return 0;
    }

    reg32(SAADC_BASE, SAADC_TASKS_SAMPLE) = 1UL;
    if (!waitForSaadcEvent(SAADC_EVENTS_END)) {
        reg32(SAADC_BASE, SAADC_TASKS_STOP) = 1UL;
        waitForSaadcEvent(SAADC_EVENTS_STOPPED);
        reg32(SAADC_BASE, SAADC_ENABLE) = SAADC_ENABLE_DISABLED;
        return 0;
    }

    reg32(SAADC_BASE, SAADC_TASKS_STOP) = 1UL;
    waitForSaadcEvent(SAADC_EVENTS_STOPPED);
    reg32(SAADC_BASE, SAADC_ENABLE) = SAADC_ENABLE_DISABLED;

    if (sample < 0) {
        sample = 0;
    }

    return scaleFromBitDepth(static_cast<int>(sample), hardwareBits, g_analogReadResolutionBits);
}

int analogReadFromPsel(uint32_t psel) {
    return analogReadFromPselWithConfig(psel, g_analogReferenceMode, g_analogGain, g_analogAcquisitionTimeUs);
}

uint32_t adcConfiguredMaxValue() {
    return (1UL << g_analogReadResolutionBits) - 1UL;
}

uint32_t adcReadingToMillivolts(int reading) {
    const uint32_t fullScaleMillivolts = adcFullScaleMillivolts(INTERNAL, NRF_ADC_GAIN_1_6);
    const uint32_t maxValue = adcConfiguredMaxValue();
    const uint32_t clipped = static_cast<uint32_t>(constrain(reading, 0, static_cast<int>(maxValue)));
    return (clipped * fullScaleMillivolts + (maxValue / 2UL)) / maxValue;
}

inline uint32_t gpioteEventOffset(uint8_t channel) {
    return GPIOTE_EVENTS_IN_BASE + (static_cast<uint32_t>(channel) * 4UL);
}

inline uint32_t gpioteConfigOffset(uint8_t channel) {
    return GPIOTE_CONFIG_BASE + (static_cast<uint32_t>(channel) * 4UL);
}

inline uint32_t gpiotePolarityForMode(int mode) {
    switch (mode) {
        case RISING:
            return GPIOTE_POLARITY_LOTOHI;
        case FALLING:
            return GPIOTE_POLARITY_HITOLO;
        case CHANGE:
        default:
            return GPIOTE_POLARITY_TOGGLE;
    }
}

inline void enableNvicIrq(uint32_t irqNumber) {
    reg32(NVIC_ISER_BASE, (irqNumber / 32UL) * 4UL) = 1UL << (irqNumber % 32UL);
}

inline void disableNvicIrq(uint32_t irqNumber) {
    reg32(NVIC_ICER_BASE, (irqNumber / 32UL) * 4UL) = 1UL << (irqNumber % 32UL);
}

uint8_t findFreeGpioteChannel() {
    for (uint8_t channel = 0; channel < 8; ++channel) {
        if (g_gpiotePins[channel] == 0xFF) {
            return channel;
        }
    }
    return 0xFF;
}

bool anyGpioteHandlerEnabled() {
    for (uint8_t channel = 0; channel < 8; ++channel) {
        if (g_gpiotePins[channel] != 0xFF) {
            return true;
        }
    }
    return false;
}

inline uint32_t pwmPselOffset(uint8_t channel) {
    return PWM_PSEL_OUT0 + (static_cast<uint32_t>(channel) * 4UL);
}

uint16_t pwmSequenceValueFromDuty(uint16_t duty) {
    if (duty > g_pwmCounterTop) {
        duty = g_pwmCounterTop;
    }
    return static_cast<uint16_t>(g_pwmCounterTop - duty) | PWM_POLARITY_INVERTED;
}

uint32_t pwmCounterClockHzForPrescaler(uint8_t prescaler) {
    return PWM_BASE_CLOCK_HZ >> prescaler;
}

uint32_t pwmFrequencyHzForPrescaler(uint8_t prescaler) {
    return pwmCounterClockHzForPrescaler(prescaler) / (static_cast<uint32_t>(g_pwmCounterTop) + 1UL);
}

uint32_t pwmFrequencyHzForConfig(uint8_t prescaler, uint16_t counterTop) {
    return pwmCounterClockHzForPrescaler(prescaler) / (static_cast<uint32_t>(counterTop) + 1UL);
}

uint8_t pwmEffectiveResolutionBitsForCounterTop(uint16_t counterTop) {
    uint32_t steps = static_cast<uint32_t>(counterTop) + 1UL;
    uint8_t bits = 0U;
    uint32_t capacity = 1UL;
    while (capacity < steps && bits < 15U) {
        capacity <<= 1U;
        ++bits;
    }
    return bits;
}

bool selectPwmFrequencyConfig(uint32_t hz, uint8_t *prescalerOut, uint16_t *counterTopOut) {
    if (hz == 0UL || prescalerOut == nullptr || counterTopOut == nullptr) {
        return false;
    }

    const uint32_t minHz = pwmFrequencyHzForConfig(PWM_PRESCALER_MAX, PWM_COUNTERTOP_MAX);
    const uint32_t maxHz = pwmFrequencyHzForConfig(0U, PWM_COUNTERTOP_MIN);
    if (hz < minHz || hz > maxHz) {
        return false;
    }

    uint64_t bestError = UINT64_MAX;
    uint8_t bestPrescaler = 0U;
    uint16_t bestCounterTop = g_pwmCounterTop;

    for (uint8_t prescaler = 0U; prescaler <= PWM_PRESCALER_MAX; ++prescaler) {
        const uint32_t clockHz = pwmCounterClockHzForPrescaler(prescaler);
        const uint32_t minCounts = static_cast<uint32_t>(PWM_COUNTERTOP_MIN) + 1UL;
        const uint32_t maxCounts = static_cast<uint32_t>(PWM_COUNTERTOP_MAX) + 1UL;
        uint32_t periodCounts = (clockHz + (hz / 2UL)) / hz;
        if (periodCounts < minCounts) {
            periodCounts = minCounts;
        }
        if (periodCounts > maxCounts) {
            periodCounts = maxCounts;
        }

        for (int delta = -1; delta <= 1; ++delta) {
            const int64_t candidateCounts = static_cast<int64_t>(periodCounts) + delta;
            if (candidateCounts < static_cast<int64_t>(minCounts) || candidateCounts > static_cast<int64_t>(maxCounts)) {
                continue;
            }

            const uint16_t candidateCounterTop = static_cast<uint16_t>(candidateCounts - 1LL);
            const uint32_t actualHz = pwmFrequencyHzForConfig(prescaler, candidateCounterTop);
            uint64_t error = 0ULL;
            if (actualHz > hz) {
                error = static_cast<uint64_t>(actualHz - hz);
            } else {
                error = static_cast<uint64_t>(hz - actualHz);
            }

            if (error < bestError || (error == bestError && candidateCounterTop > bestCounterTop)) {
                bestError = error;
                bestPrescaler = prescaler;
                bestCounterTop = candidateCounterTop;
            }
        }
    }

    *prescalerOut = bestPrescaler;
    *counterTopOut = bestCounterTop;
    return true;
}

uint16_t scaleToPwmCounterTop(int value, uint8_t bits) {
    if (bits == 0U) {
        return 0U;
    }

    const uint32_t maxDuty = static_cast<uint32_t>(g_pwmCounterTop);
    const uint32_t maxValue = (1UL << bits) - 1UL;
    const uint32_t clipped = static_cast<uint32_t>(constrain(value, 0, static_cast<int>(maxValue)));
    return static_cast<uint16_t>((clipped * maxDuty + (maxValue / 2UL)) / maxValue);
}

int scaleFrom10Bit(int value, uint8_t bits) {
    if (bits == 10U) {
        return constrain(value, 0, 1023);
    }

    const uint32_t maxValue = (1UL << bits) - 1UL;
    return static_cast<int>((static_cast<uint32_t>(constrain(value, 0, 1023)) * maxValue + 511UL) / 1023UL);
}

int scaleFromBitDepth(int value, uint8_t fromBits, uint8_t toBits) {
    const uint32_t fromMax = (1UL << fromBits) - 1UL;
    const uint32_t toMax = (1UL << toBits) - 1UL;
    return static_cast<int>((static_cast<uint32_t>(constrain(value, 0, static_cast<int>(fromMax))) * toMax + (fromMax / 2UL)) / fromMax);
}

uint8_t adcHardwareResolutionBits() {
    if (g_analogReadResolutionBits <= 8U) {
        return 8U;
    }
    if (g_analogReadResolutionBits <= 10U) {
        return 10U;
    }
    if (g_analogReadResolutionBits <= 12U) {
        return 12U;
    }
    return 14U;
}

uint32_t saadcResolutionRegisterValue(uint8_t bits) {
    switch (bits) {
        case 8U:
            return 0UL;
        case 10U:
            return 1UL;
        case 12U:
            return 2UL;
        case 14U:
        default:
            return 3UL;
    }
}

void detachPwmPin(uint8_t pin) {
    if (pin >= sizeof(g_pwmSlots)) {
        return;
    }
    const uint8_t slot = g_pwmSlots[pin];
    if (slot >= 4U) {
        return;
    }
    g_pwmPins[slot] = 0xFF;
    g_pwmSlots[pin] = 0xFF;
}

void updatePwmHardware() {
    reg32(PWM0_BASE, PWM_TASKS_STOP) = 1UL;
    reg32(PWM0_BASE, PWM_ENABLE) = PWM_ENABLE_DISABLED;

    bool anyActive = false;
    for (uint8_t slot = 0; slot < 4U; ++slot) {
        const uint8_t pin = g_pwmPins[slot];
        if (pin == 0xFF) {
            g_pwmSequence[slot] = pwmSequenceValueFromDuty(0U);
            reg32(PWM0_BASE, pwmPselOffset(slot)) = PWM_PSEL_DISCONNECTED;
            continue;
        }

        const uint32_t rawPin = rawPinFor(pin);
        if (!isValidRawPin(rawPin)) {
            g_pwmPins[slot] = 0xFF;
            g_pwmSequence[slot] = pwmSequenceValueFromDuty(0U);
            reg32(PWM0_BASE, pwmPselOffset(slot)) = PWM_PSEL_DISCONNECTED;
            continue;
        }

        anyActive = true;
        reg32(PWM0_BASE, pwmPselOffset(slot)) = rawPin;
        g_pwmSequence[slot] = pwmSequenceValueFromDuty(g_pwmValues[pin]);
    }

    if (!anyActive) {
        return;
    }

    reg32(PWM0_BASE, PWM_MODE) = 0UL;
    reg32(PWM0_BASE, PWM_PRESCALER) = g_pwmPrescaler;
    reg32(PWM0_BASE, PWM_COUNTERTOP) = static_cast<uint32_t>(g_pwmCounterTop);
    reg32(PWM0_BASE, PWM_DECODER) = PWM_DECODER_LOAD_INDIVIDUAL;
    reg32(PWM0_BASE, PWM_LOOP) = 0UL;
    reg32(PWM0_BASE, PWM_SEQ0_PTR) = reinterpret_cast<uint32_t>(&g_pwmSequence[0]);
    reg32(PWM0_BASE, PWM_SEQ0_CNT) = 4UL;
    reg32(PWM0_BASE, PWM_SEQ0_REFRESH) = 0UL;
    reg32(PWM0_BASE, PWM_SEQ0_ENDDELAY) = 0UL;
    reg32(PWM0_BASE, PWM_ENABLE) = PWM_ENABLE_ENABLED;
    reg32(PWM0_BASE, PWM_TASKS_SEQSTART0) = 1UL;
}

uint8_t assignPwmSlot(uint8_t pin) {
    if (pin >= sizeof(g_pwmSlots)) {
        return 0xFF;
    }

    if (g_pwmSlots[pin] < 4U) {
        return g_pwmSlots[pin];
    }

    for (uint8_t slot = 0; slot < 4U; ++slot) {
        if (g_pwmPins[slot] == 0xFF) {
            g_pwmPins[slot] = pin;
            g_pwmSlots[pin] = slot;
            return slot;
        }
    }

    return 0xFF;
}

uint32_t nextRandomWord() {
    if (g_randomState == 0U) {
        g_randomState = 0xA341316CUL;
    }
    uint32_t x = g_randomState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_randomState = x;
    return x;
}
}

// Yield hook for background maintenance. Auto-rescue / WDT-based recovery was
// intentionally removed for this board family, but yield() still calls the
// startup-level hook so future board profiles can reintroduce lightweight
// maintenance without changing the main loop contract.
extern "C" void nrfWdtFeed(void);

extern "C" void SysTick_Handler(void) {
    ++g_millis;
}

void init(void) {
    g_millis = 0;
    SysTickRegisters &timer = systick();
    timer.LOAD = systickReload();
    timer.VAL = 0;
    timer.CTRL = 0x07UL;
    nrfGdbStub().init();
}

void initVariant(void) {
    nrfUsbSerialBackend().configureFromSystemProfile();
}

void yield(void) {
    nrfUsbSerialBackend().poll();
    nrfWdtFeed();
}

void delay(unsigned long milliseconds) {
    unsigned long start = millis();
    while ((millis() - start) < milliseconds) {
        yield();
    }
}

void delayMicroseconds(unsigned int microseconds) {
    while (microseconds >= 1000U) {
        const unsigned long start = micros();
        while ((micros() - start) < 1000UL) {
            yield();
        }
        microseconds = static_cast<unsigned int>(microseconds - 1000U);
    }

    const unsigned long start = micros();
    while ((micros() - start) < microseconds) {
        __asm__ volatile("nop");
    }
}

unsigned long millis(void) {
    return g_millis;
}

unsigned long micros(void) {
    SysTickRegisters &timer = systick();
    const uint32_t reload = timer.LOAD + 1UL;
    uint32_t millisSnapshot;
    uint32_t valueSnapshot;
    uint32_t ctrlSnapshot;

    do {
        millisSnapshot = g_millis;
        valueSnapshot = timer.VAL;
        ctrlSnapshot = timer.CTRL;
    } while (millisSnapshot != g_millis);

    uint32_t elapsedCycles = reload - valueSnapshot;
    if ((ctrlSnapshot & (1UL << 16)) != 0UL && elapsedCycles < reload) {
        ++millisSnapshot;
        elapsedCycles = reload - timer.VAL;
    }

    return (millisSnapshot * 1000UL) + (elapsedCycles / (F_CPU / 1000000UL));
}

void pinMode(uint8_t pin, uint8_t mode) {
    if (pin >= sizeof(g_pinModes)) {
        return;
    }

    const uint32_t rawPin = rawPinFor(pin);
    if (!isValidRawPin(rawPin)) {
        return;
    }

    GpioRegisters &port = gpioPort(portIndexFor(rawPin));
    const uint32_t bitMask = 1UL << bitIndexFor(rawPin);

    g_pinModes[pin] = mode;
    port.PIN_CNF[bitIndexFor(rawPin)] = pinConfigurationFor(mode);
    if (mode == OUTPUT) {
        port.DIRSET = bitMask;
    } else {
        port.DIRCLR = bitMask;
    }
}

void digitalWrite(uint8_t pin, uint8_t value) {
    if (pin >= sizeof(g_pinValues)) {
        return;
    }

    const uint32_t rawPin = rawPinFor(pin);
    if (!isValidRawPin(rawPin)) {
        return;
    }

    GpioRegisters &port = gpioPort(portIndexFor(rawPin));
    const uint32_t bitMask = 1UL << bitIndexFor(rawPin);

    detachPwmPin(pin);
    updatePwmHardware();

    g_pinValues[pin] = value;
    if (value == LOW) {
        port.OUTCLR = bitMask;
    } else {
        port.OUTSET = bitMask;
    }
}

int digitalRead(uint8_t pin) {
    if (pin >= sizeof(g_pinValues)) {
        return LOW;
    }

    const uint32_t rawPin = rawPinFor(pin);
    if (!isValidRawPin(rawPin)) {
        return g_pinValues[pin];
    }

    const GpioRegisters &port = gpioPort(portIndexFor(rawPin));
    const uint32_t bitMask = 1UL << bitIndexFor(rawPin);
    if ((port.IN & bitMask) != 0U) {
        return HIGH;
    }
    return LOW;
}

void attachInterrupt(uint8_t pin, voidFuncPtr callback, int mode) {
    if (pin >= sizeof(g_interruptHandlers) / sizeof(g_interruptHandlers[0])) {
        return;
    }

    const uint32_t rawPin = rawPinFor(pin);
    if (!isValidRawPin(rawPin)) {
        return;
    }

    uint8_t channel = g_interruptChannels[pin];
    if (channel == 0xFF) {
        channel = findFreeGpioteChannel();
        if (channel == 0xFF) {
            return;
        }
        g_interruptChannels[pin] = channel;
        g_gpiotePins[channel] = pin;
    }

    g_interruptHandlers[pin] = callback;
    g_interruptModes[pin] = static_cast<uint8_t>(mode);
    reg32(GPIOTE_BASE, gpioteEventOffset(channel)) = 0UL;
    reg32(GPIOTE_BASE, gpioteConfigOffset(channel)) =
        GPIOTE_MODE_EVENT |
        (rawPin << 8) |
        (gpiotePolarityForMode(mode) << 16);
    reg32(GPIOTE_BASE, GPIOTE_INTENSET) = 1UL << channel;
    enableNvicIrq(GPIOTE_IRQ_NUMBER);
}

void detachInterrupt(uint8_t pin) {
    if (pin >= sizeof(g_interruptHandlers) / sizeof(g_interruptHandlers[0])) {
        return;
    }

    const uint8_t channel = g_interruptChannels[pin];
    g_interruptHandlers[pin] = nullptr;
    g_interruptModes[pin] = 0;
    g_interruptChannels[pin] = 0xFF;

    if (channel != 0xFF) {
        reg32(GPIOTE_BASE, GPIOTE_INTENCLR) = 1UL << channel;
        reg32(GPIOTE_BASE, gpioteConfigOffset(channel)) = 0UL;
        reg32(GPIOTE_BASE, gpioteEventOffset(channel)) = 0UL;
        g_gpiotePins[channel] = 0xFF;
    }

    if (!anyGpioteHandlerEnabled()) {
        disableNvicIrq(GPIOTE_IRQ_NUMBER);
    }
}

void analogWrite(uint8_t pin, int value) {
    if (pin >= sizeof(g_pwmValues)) {
        g_pwmLastWriteStatus = NRF_PWM_WRITE_INVALID_PIN;
        return;
    }

    const uint32_t rawPin = rawPinFor(pin);
    if (!isValidRawPin(rawPin)) {
        g_pwmLastWriteStatus = NRF_PWM_WRITE_INVALID_PIN;
        return;
    }

    if (!nrfDigitalPinHasPwm(pin)) {
        g_pwmLastWriteStatus = NRF_PWM_WRITE_UNSUPPORTED_PIN;
        return;
    }

    const uint16_t scaledValue = scaleToPwmCounterTop(value, g_analogWriteResolutionBits);
    g_pwmValues[pin] = scaledValue;
    g_pwmLastWriteStatus = NRF_PWM_WRITE_OK;

    if (scaledValue == 0U) {
        detachPwmPin(pin);
        updatePwmHardware();
        digitalWrite(pin, LOW);
        return;
    }

    if (scaledValue >= g_pwmCounterTop) {
        detachPwmPin(pin);
        updatePwmHardware();
        digitalWrite(pin, HIGH);
        return;
    }

    if (assignPwmSlot(pin) == 0xFF) {
        g_pwmLastWriteStatus = NRF_PWM_WRITE_CHANNEL_EXHAUSTED;
        return;
    }

    pinMode(pin, OUTPUT);
    updatePwmHardware();
}

int analogRead(uint8_t pin) {
    if (pin >= sizeof(g_pinValues)) {
        return 0;
    }

    const uint32_t rawPin = rawPinFor(pin);
    if (!isValidRawPin(rawPin)) {
        return 0;
    }

    if (!isAnalogCapableRawPin(rawPin)) {
        return 0;
    }

    return analogReadFromPsel(saadcPselForRawPin(rawPin));
}

void analogReference(uint8_t mode) {
    (void)nrfAdcSetReference(mode);
}

int analogReadVDD(void) {
    return analogReadFromPselWithConfig(SAADC_CH_PSEL_VDD, INTERNAL, NRF_ADC_GAIN_1_6, 10U);
}

int analogReadVDDHDIV5(void) {
    return analogReadFromPselWithConfig(SAADC_CH_PSEL_VDDHDIV5, INTERNAL, NRF_ADC_GAIN_1_6, 10U);
}

bool nrfBatteryReadingSupported(void) {
    return nrfBoardHasBatterySense();
}

int nrfBatteryRaw(void) {
    if (!nrfBoardHasBatterySense()) {
        return 0;
    }

    if (nrfBoardBatterySenseViaVddhDiv5()) {
        return analogReadVDDHDIV5();
    }

    const NrfBoardPowerInfo power = nrfBoardPowerInfo();
    if (power.batterySensePin == 0xFFU) {
        return 0;
    }

    return analogRead(power.batterySensePin);
}

uint32_t nrfBatteryMillivolts(void) {
    if (!nrfBoardHasBatterySense()) {
        return 0UL;
    }

    const NrfBoardPowerInfo power = nrfBoardPowerInfo();
    const uint32_t millivolts = adcReadingToMillivolts(nrfBatteryRaw());
    uint32_t numerator = power.batteryVoltageScaleNumerator;
    if (numerator == 0U) {
        numerator = 1U;
    }
    uint32_t denominator = power.batteryVoltageScaleDenominator;
    if (denominator == 0U) {
        denominator = 1U;
    }
    return (millivolts * numerator + (denominator / 2UL)) / denominator;
}

uint8_t nrfBatteryVoltageScaleNumerator(void) {
    return nrfBoardBatteryVoltageScaleNumerator();
}

uint8_t nrfBatteryVoltageScaleDenominator(void) {
    return nrfBoardBatteryVoltageScaleDenominator();
}

bool nrfUsbPowerPresent(void) {
    return nrfBoardUsbPowerPresent();
}

bool nrfUsbBatteryCoexistencePossible(void) {
    return nrfBoardUsbBatteryCoexistencePossible();
}

bool nrfUsbBatteryCoexistenceActive(void) {
    return nrfBoardUsbBatteryCoexistenceActive();
}

bool nrfSystemSleepSupported(void) {
    return true;
}

bool nrfSystemPowerDownSupported(void) {
    return true;
}

bool nrfSystemUsbBlocksLowPower(void) {
    return nrfUsbRuntimeEnabled() && USBDevice.configured() && !USBDevice.suspended();
}

bool nrfSystemCanSleep(void) {
    return nrfSystemSleepSupported() && !nrfSystemUsbBlocksLowPower();
}

bool nrfSystemCanPowerDown(void) {
    return nrfSystemPowerDownSupported() && !nrfSystemUsbBlocksLowPower();
}

void nrfSystemSleep(void) {
    if (!nrfSystemCanSleep()) {
        return;
    }
    __asm__ volatile("wfi" : : : "memory");
}

void nrfSystemPowerDown(void) {
    if (!nrfSystemCanPowerDown()) {
        return;
    }
    reg32(POWER_BASE, POWER_SYSTEMOFF) = 1UL;
    __asm__ volatile("dsb" : : : "memory");
    __asm__ volatile("wfi" : : : "memory");
}

void analogReadResolution(int bits) {
    g_analogReadResolutionBits = static_cast<uint8_t>(constrain(bits, 1, 14));
}

void analogWriteResolution(int bits) {
    g_analogWriteResolutionBits = static_cast<uint8_t>(constrain(bits, 1, 16));
}

uint8_t shiftIn(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder) {
    uint8_t value = 0U;
    for (uint8_t bitIndex = 0; bitIndex < 8U; ++bitIndex) {
        digitalWrite(clockPin, HIGH);
        delayMicroseconds(1U);
        uint8_t sampled = 0U;
        if (digitalRead(dataPin) == HIGH) {
            sampled = 1U;
        }
        if (bitOrder == LSBFIRST) {
            value |= static_cast<uint8_t>(sampled << bitIndex);
        } else {
            value |= static_cast<uint8_t>(sampled << (7U - bitIndex));
        }
        digitalWrite(clockPin, LOW);
        delayMicroseconds(1U);
    }
    return value;
}

void shiftOut(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder, uint8_t value) {
    for (uint8_t bitIndex = 0; bitIndex < 8U; ++bitIndex) {
        uint8_t mask = static_cast<uint8_t>(1U << (7U - bitIndex));
        if (bitOrder == LSBFIRST) {
            mask = static_cast<uint8_t>(1U << bitIndex);
        }
        if ((value & mask) != 0U) {
            digitalWrite(dataPin, HIGH);
        } else {
            digitalWrite(dataPin, LOW);
        }
        delayMicroseconds(1U);
        digitalWrite(clockPin, HIGH);
        delayMicroseconds(1U);
        digitalWrite(clockPin, LOW);
    }
}

long random(long max) {
    if (max <= 0) {
        return 0;
    }
    return static_cast<long>(nextRandomWord() % static_cast<uint32_t>(max));
}

long random(long min, long max) {
    if (max <= min) {
        return min;
    }
    return min + random(max - min);
}

void randomSeed(unsigned long seed) {
    if (seed == 0UL) {
        g_randomState = static_cast<uint32_t>(micros());
    } else {
        g_randomState = static_cast<uint32_t>(seed);
    }
}

uint32_t pulseIn(uint8_t pin, uint8_t state, unsigned long timeout) {
    int targetState = HIGH;
    if (state == LOW) {
        targetState = LOW;
    }
    const unsigned long start = micros();

    while (digitalRead(pin) == targetState) {
        if ((micros() - start) >= timeout) {
            return 0U;
        }
    }

    while (digitalRead(pin) != targetState) {
        if ((micros() - start) >= timeout) {
            return 0U;
        }
    }

    const unsigned long pulseStart = micros();
    while (digitalRead(pin) == targetState) {
        if ((micros() - start) >= timeout) {
            return 0U;
        }
    }

    return micros() - pulseStart;
}

uint32_t pulseInLong(uint8_t pin, uint8_t state, unsigned long timeout) {
    return pulseIn(pin, state, timeout);
}

long map(long value, long inMin, long inMax, long outMin, long outMax) {
    if (inMax == inMin) {
        return outMin;
    }
    return (value - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}

void __enable_irq_stub(void) {
    __asm__ volatile("cpsie i" : : : "memory");
}

void __disable_irq_stub(void) {
    __asm__ volatile("cpsid i" : : : "memory");
}

uint8_t nrfPwmChannelCapacity(void) {
    return 4U;
}

uint8_t nrfPwmActiveChannels(void) {
    uint8_t active = 0U;
    for (uint8_t slot = 0; slot < 4U; ++slot) {
        if (g_pwmPins[slot] != 0xFF) {
            ++active;
        }
    }
    return active;
}

bool nrfDigitalPinHasPwm(uint8_t pin) {
    return nrfBoardPinSupportsPwm(pin);
}

bool nrfPwmChannelsIndependent(void) {
    return true;
}

bool nrfPwmSharedTimer(void) {
    return true;
}

bool nrfPwmIndependentTimersSupported(void) {
    return false;
}

uint8_t nrfPwmTimerGroupCount(void) {
    return 1U;
}

bool nrfPwmCanAllocateChannel(uint8_t pin) {
    return pin < sizeof(g_pwmSlots) && nrfDigitalPinHasPwm(pin) && (g_pwmSlots[pin] < 4U || nrfPwmActiveChannels() < nrfPwmChannelCapacity());
}

bool nrfPwmPolarityConfigurable(void) {
    return false;
}

bool nrfPwmCenterAlignedSupported(void) {
    return false;
}

bool nrfPwmFrequencyConfigurable(void) {
    return true;
}

uint32_t nrfPwmMinFrequencyHz(void) {
    return pwmFrequencyHzForConfig(PWM_PRESCALER_MAX, PWM_COUNTERTOP_MAX);
}

uint32_t nrfPwmMaxFrequencyHz(void) {
    return pwmFrequencyHzForConfig(0U, PWM_COUNTERTOP_MIN);
}

bool nrfPwmSupportsFrequency(uint32_t hz) {
    uint8_t prescaler = 0U;
    uint16_t counterTop = 0U;
    return selectPwmFrequencyConfig(hz, &prescaler, &counterTop);
}

bool nrfPwmSetFrequency(uint32_t hz) {
    uint8_t prescaler = 0U;
    uint16_t counterTop = 0U;
    if (!selectPwmFrequencyConfig(hz, &prescaler, &counterTop)) {
        return false;
    }

    g_pwmPrescaler = prescaler;
    g_pwmCounterTop = counterTop;
    if (nrfPwmActiveChannels() > 0U) {
        updatePwmHardware();
    }
    return true;
}

bool analogWriteFrequency(uint32_t hz) {
    return nrfPwmSetFrequency(hz);
}

bool analogWritePeriodUs(uint32_t periodUs) {
    if (periodUs == 0UL) {
        return false;
    }
    const uint32_t hz = static_cast<uint32_t>((1000000ULL + (periodUs / 2ULL)) / periodUs);
    return analogWriteFrequency(hz);
}

bool analogWritePeriodMs(uint32_t periodMs) {
    if (periodMs == 0UL) {
        return false;
    }
    const uint32_t periodUs = periodMs * 1000UL;
    return analogWritePeriodUs(periodUs);
}

uint32_t analogWriteFrequencyHz(void) {
    return nrfPwmFrequencyHz();
}

NrfPwmWriteStatus nrfPwmLastWriteStatus(void) {
    return g_pwmLastWriteStatus;
}

const char *nrfPwmWriteStatusName(NrfPwmWriteStatus status) {
    switch (status) {
        case NRF_PWM_WRITE_OK:
            return "ok";
        case NRF_PWM_WRITE_INVALID_PIN:
            return "invalid-pin";
        case NRF_PWM_WRITE_UNSUPPORTED_PIN:
            return "unsupported-pin";
        case NRF_PWM_WRITE_CHANNEL_EXHAUSTED:
            return "channel-exhausted";
        default:
            return "unknown";
    }
}

uint32_t nrfPwmCounterClockHz(void) {
    return pwmCounterClockHzForPrescaler(g_pwmPrescaler);
}

uint32_t nrfPwmFrequencyHz(void) {
    return pwmFrequencyHzForConfig(g_pwmPrescaler, g_pwmCounterTop);
}

uint16_t nrfPwmCounterTop(void) {
    return g_pwmCounterTop;
}

uint8_t nrfPwmNativeResolutionBits(void) {
    return 15U;
}

uint8_t nrfPwmEffectiveResolutionBits(void) {
    return pwmEffectiveResolutionBitsForCounterTop(g_pwmCounterTop);
}

uint8_t nrfPwmConfiguredResolutionBits(void) {
    return g_analogWriteResolutionBits;
}

PwmState PwmNow(void) {
    return {
        nrfPwmChannelCapacity(),
        nrfPwmActiveChannels(),
        nrfPwmNativeResolutionBits(),
        nrfPwmConfiguredResolutionBits(),
        nrfPwmSharedTimer(),
        nrfPwmIndependentTimersSupported()
    };
}

bool PwmBegin(uint8_t bits) {
    analogWriteResolution(bits);
    return nrfPwmConfiguredResolutionBits() == bits;
}

uint8_t PwmCap(void) {
    return nrfPwmChannelCapacity();
}

uint8_t PwmUsed(void) {
    return nrfPwmActiveChannels();
}

bool PwmShared(void) {
    return nrfPwmSharedTimer();
}

bool PwmSplitTimers(void) {
    return nrfPwmIndependentTimersSupported();
}

uint8_t PwmBitsNative(void) {
    return nrfPwmNativeResolutionBits();
}

uint8_t PwmBitsConfig(void) {
    return nrfPwmConfiguredResolutionBits();
}

bool nrfAnalogInputSupported(uint8_t pin) {
    const uint32_t rawPin = rawPinFor(pin);
    return isValidRawPin(rawPin) && isAnalogCapableRawPin(rawPin);
}

bool nrfAdcPresent(void) {
    return true;
}

uint8_t nrfAdcChannelCount(void) {
    return 8U;
}

uint8_t nrfAdcNativeResolutionBits(void) {
    return 14U;
}

uint8_t nrfAdcConfiguredResolutionBits(void) {
    return g_analogReadResolutionBits;
}

bool nrfAdcReferenceConfigurable(void) {
    return true;
}

uint8_t nrfAdcReference(void) {
    return g_analogReferenceMode;
}

bool nrfAdcSetReference(uint8_t reference) {
    if (!saadcReferenceSupported(reference)) {
        return false;
    }

    g_analogReferenceMode = normalizeSaadcReference(reference);
    return true;
}

bool nrfAdcGainConfigurable(void) {
    return true;
}

NrfAdcGain nrfAdcGain(void) {
    return g_analogGain;
}

bool nrfAdcSetGain(NrfAdcGain gain) {
    if (!saadcGainSupported(gain)) {
        return false;
    }

    g_analogGain = gain;
    return true;
}

bool nrfAdcAcquisitionTimeConfigurable(void) {
    return true;
}

uint8_t nrfAdcAcquisitionTimeUs(void) {
    return g_analogAcquisitionTimeUs;
}

bool nrfAdcSetAcquisitionTimeUs(uint8_t microseconds) {
    if (!saadcAcquisitionTimeSupported(microseconds)) {
        return false;
    }

    g_analogAcquisitionTimeUs = microseconds;
    return true;
}

bool nrfAdcCalibrationSupported(void) {
    return true;
}

bool nrfAdcCalibrationPending(void) {
    return saadcConfigNeedsCalibration(g_analogReferenceMode, g_analogGain, g_analogAcquisitionTimeUs);
}

bool nrfAdcCalibrateOffset(void) {
    return calibrateSaadc(g_analogReferenceMode, g_analogGain, g_analogAcquisitionTimeUs);
}

bool nrfDacPresent(void) {
    return false;
}

uint8_t nrfDacChannelCount(void) {
    return 0U;
}

bool nrfToneSupported(void) {
    return false;
}

bool nrfServoSupported(void) {
    return false;
}

uint32_t nrfCpuFrequencyHz(void) {
    return static_cast<uint32_t>(F_CPU);
}

bool nrfCpuOverclockSupported(void) {
    return false;
}

bool nrfCpuOverclockEnabled(void) {
    return false;
}

extern "C" void GPIOTE_IRQHandler(void) {
    for (uint8_t channel = 0; channel < 8; ++channel) {
        if (reg32(GPIOTE_BASE, gpioteEventOffset(channel)) == 0UL) {
            continue;
        }

        reg32(GPIOTE_BASE, gpioteEventOffset(channel)) = 0UL;
        const uint8_t pin = g_gpiotePins[channel];
        if (pin >= sizeof(g_interruptHandlers) / sizeof(g_interruptHandlers[0])) {
            continue;
        }

        voidFuncPtr callback = g_interruptHandlers[pin];
        if (callback != nullptr) {
            callback();
        }
    }
}
