#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HIGH 0x1
#define LOW  0x0

#define INPUT 0x0
#define OUTPUT 0x1
#define INPUT_PULLUP 0x2
#define INPUT_PULLDOWN 0x3

#define LSBFIRST 0
#define MSBFIRST 1

#define CHANGE 1
#define FALLING 2
#define RISING 3

#define DEFAULT 1
#define EXTERNAL 0
#define INTERNAL 2
#define INTERNAL1V2 3
#define AR_VDD4 4

#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

#define PI 3.1415926535897932384626433832795
#define HALF_PI 1.5707963267948966192313216916398
#define TWO_PI 6.283185307179586476925286766559
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#define radians(deg) ((deg) * DEG_TO_RAD)
#define degrees(rad) ((rad) * RAD_TO_DEG)
#define sq(value) ((value) * (value))
#define WAIT(expr) while (!(expr)) { delay(1000); }

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitToggle(value, bit) ((value) ^= (1UL << (bit)))
#define bitWrite(value, bit, bitvalue) ((bitvalue) ? bitSet(value, bit) : bitClear(value, bit))
#define bit(b) (1UL << (b))

#define clockCyclesPerMicrosecond() (F_CPU / 1000000UL)
#define clockCyclesToMicroseconds(cycles) ((cycles) / clockCyclesPerMicrosecond())
#define microsecondsToClockCycles(microseconds) ((microseconds) * clockCyclesPerMicrosecond())

#define interrupts() __enable_irq_stub()
#define noInterrupts() __disable_irq_stub()

#define lowByte(w) ((uint8_t) ((w) & 0xff))
#define highByte(w) ((uint8_t) ((w) >> 8))

#define digitalPinToInterrupt(pin) (pin)
#define analogInputToDigitalPin(pin) ((pin) + PIN_A0)
#define digitalPinHasPWM(pin) (nrfDigitalPinHasPwm(pin))

#define NOT_A_PIN 0xFFu
#define NUM_DIGITAL_PINS PINS_COUNT
#define NUM_ANALOG_INPUTS 8u

typedef bool boolean;
typedef uint8_t byte;
typedef void (*voidFuncPtr)(void);
typedef void (*voidFuncPtrParam)(void *);

typedef enum {
	NRF_PWM_WRITE_OK = 0,
	NRF_PWM_WRITE_INVALID_PIN = 1,
	NRF_PWM_WRITE_UNSUPPORTED_PIN = 2,
	NRF_PWM_WRITE_CHANNEL_EXHAUSTED = 3,
} NrfPwmWriteStatus;

typedef enum {
	NRF_ADC_GAIN_1_6 = 0,
	NRF_ADC_GAIN_1_5 = 1,
	NRF_ADC_GAIN_1_4 = 2,
	NRF_ADC_GAIN_1_3 = 3,
	NRF_ADC_GAIN_1_2 = 4,
	NRF_ADC_GAIN_1 = 5,
	NRF_ADC_GAIN_2 = 6,
	NRF_ADC_GAIN_4 = 7,
} NrfAdcGain;

typedef struct {
	uint8_t cap;
	uint8_t active;
	uint8_t bitsNative;
	uint8_t bitsConfig;
	bool shared;
	bool splitTimers;
} PwmState;

void init(void);
void initVariant(void);
void yield(void);
void delay(unsigned long milliseconds);
void delayMicroseconds(unsigned int microseconds);
unsigned long millis(void);
unsigned long micros(void);
uint32_t pulseInLong(uint8_t pin, uint8_t state, unsigned long timeout = 1000000UL);

void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t value);
int digitalRead(uint8_t pin);
void attachInterrupt(uint8_t pin, voidFuncPtr callback, int mode);
void detachInterrupt(uint8_t pin);

void analogWrite(uint8_t pin, int value);
bool analogWriteFrequency(uint32_t hz);
bool analogWritePeriodUs(uint32_t periodUs);
bool analogWritePeriodMs(uint32_t periodMs);
uint32_t analogWriteFrequencyHz(void);
int analogRead(uint8_t pin);
void analogReference(uint8_t mode);
int analogReadVDD(void);
int analogReadVDDHDIV5(void);
bool nrfBatteryReadingSupported(void);
int nrfBatteryRaw(void);
uint32_t nrfBatteryMillivolts(void);
uint8_t nrfBatteryVoltageScaleNumerator(void);
uint8_t nrfBatteryVoltageScaleDenominator(void);
bool nrfUsbPowerPresent(void);
bool nrfUsbBatteryCoexistencePossible(void);
bool nrfUsbBatteryCoexistenceActive(void);
bool nrfSystemSleepSupported(void);
bool nrfSystemPowerDownSupported(void);
bool nrfSystemUsbBlocksLowPower(void);
bool nrfSystemCanSleep(void);
bool nrfSystemCanPowerDown(void);
void nrfSystemSleep(void);
void nrfSystemPowerDown(void);
void analogReadResolution(int bits);
void analogWriteResolution(int bits);
uint8_t shiftIn(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder);
void shiftOut(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder, uint8_t value);
uint32_t pulseIn(uint8_t pin, uint8_t state, unsigned long timeout);
long map(long value, long inMin, long inMax, long outMin, long outMax);

uint8_t nrfPwmChannelCapacity(void);
uint8_t nrfPwmActiveChannels(void);
bool nrfDigitalPinHasPwm(uint8_t pin);
bool nrfPwmChannelsIndependent(void);
bool nrfPwmSharedTimer(void);
bool nrfPwmIndependentTimersSupported(void);
uint8_t nrfPwmTimerGroupCount(void);
bool nrfPwmCanAllocateChannel(uint8_t pin);
bool nrfPwmPolarityConfigurable(void);
bool nrfPwmCenterAlignedSupported(void);
bool nrfPwmFrequencyConfigurable(void);
uint32_t nrfPwmMinFrequencyHz(void);
uint32_t nrfPwmMaxFrequencyHz(void);
bool nrfPwmSupportsFrequency(uint32_t hz);
bool nrfPwmSetFrequency(uint32_t hz);

// Multi-module PWM extensions (nRF52 has PWM0..PWM3, each with 4 channels and
// an independent prescaler/countertop). Pins on the same module share a
// frequency group; across modules they're independent. See wiring.cpp for
// allocation semantics.
#ifndef NRF_PWM_PIN_POLARITY_HIGH_ON_DUTY
#define NRF_PWM_PIN_POLARITY_HIGH_ON_DUTY 0U
#define NRF_PWM_PIN_POLARITY_LOW_ON_DUTY  1U
#endif
bool nrfPwmSetPinFrequency(uint8_t pin, uint32_t hz);
uint32_t nrfPwmPinFrequencyHz(uint8_t pin);
uint8_t nrfPwmPinTimerGroup(uint8_t pin);    // 0..3 module index, 0xFF if pin not yet bound
bool nrfPwmSetPinPolarity(uint8_t pin, uint8_t polarity);
uint8_t nrfPwmPinPolarity(uint8_t pin);
bool nrfPwmConfigureComplementary(uint8_t pinA, uint8_t pinB, uint16_t deadTimeTicks);
NrfPwmWriteStatus nrfPwmLastWriteStatus(void);
const char *nrfPwmWriteStatusName(NrfPwmWriteStatus status);
uint32_t nrfPwmCounterClockHz(void);
uint32_t nrfPwmFrequencyHz(void);
uint16_t nrfPwmCounterTop(void);
uint8_t nrfPwmNativeResolutionBits(void);
uint8_t nrfPwmEffectiveResolutionBits(void);
uint8_t nrfPwmConfiguredResolutionBits(void);
PwmState PwmNow(void);
bool PwmBegin(uint8_t bits);
uint8_t PwmCap(void);
uint8_t PwmUsed(void);
bool PwmShared(void);
bool PwmSplitTimers(void);
uint8_t PwmBitsNative(void);
uint8_t PwmBitsConfig(void);
bool nrfAnalogInputSupported(uint8_t pin);
bool nrfAdcPresent(void);
uint8_t nrfAdcChannelCount(void);
uint8_t nrfAdcNativeResolutionBits(void);
uint8_t nrfAdcConfiguredResolutionBits(void);
bool nrfAdcReferenceConfigurable(void);
uint8_t nrfAdcReference(void);
bool nrfAdcSetReference(uint8_t reference);
bool nrfAdcGainConfigurable(void);
NrfAdcGain nrfAdcGain(void);
bool nrfAdcSetGain(NrfAdcGain gain);
bool nrfAdcAcquisitionTimeConfigurable(void);
uint8_t nrfAdcAcquisitionTimeUs(void);
bool nrfAdcSetAcquisitionTimeUs(uint8_t microseconds);
bool nrfAdcCalibrationSupported(void);
bool nrfAdcCalibrationPending(void);
bool nrfAdcCalibrateOffset(void);
bool nrfDacPresent(void);
uint8_t nrfDacChannelCount(void);
bool nrfToneSupported(void);
bool nrfServoSupported(void);
uint32_t nrfCpuFrequencyHz(void);
bool nrfCpuOverclockSupported(void);
bool nrfCpuOverclockEnabled(void);

void __enable_irq_stub(void);
void __disable_irq_stub(void);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include "Print.h"
#include "IPAddress.h"
#include "Printable.h"
#include "Stream.h"
#include "Client.h"
#include "HardwareSerial.h"
#include "Server.h"
#include "UDP.h"
#include "WString.h"
#include "NrfBoard.h"
#include "NrfDebug.h"
#include "NrfGdbStub.h"
#include "NrfSystem.h"
#include "NrfUsbd.h"
#include "NrfUsbSerial.h"
#include "NrfServiceSerial.h"
#include "PluggableUSB.h"
#include "USBDevice.h"
#include "SPI.h"
#include "Wire.h"
#include <variant.h>

long random(long max);
long random(long min, long max);
void randomSeed(unsigned long seed);

void setup(void);
void loop(void);

#ifndef SERIAL_8N1
#define SERIAL_5N1 0x00u
#define SERIAL_6N1 0x02u
#define SERIAL_7N1 0x04u
#define SERIAL_8N1 0x06u
#define SERIAL_5N2 0x08u
#define SERIAL_6N2 0x0Au
#define SERIAL_7N2 0x0Cu
#define SERIAL_8N2 0x0Eu
#define SERIAL_5E1 0x20u
#define SERIAL_6E1 0x22u
#define SERIAL_7E1 0x24u
#define SERIAL_8E1 0x26u
#define SERIAL_5E2 0x28u
#define SERIAL_6E2 0x2Au
#define SERIAL_7E2 0x2Cu
#define SERIAL_8E2 0x2Eu
#define SERIAL_5O1 0x30u
#define SERIAL_6O1 0x32u
#define SERIAL_7O1 0x34u
#define SERIAL_8O1 0x36u
#define SERIAL_5O2 0x38u
#define SERIAL_6O2 0x3Au
#define SERIAL_7O2 0x3Cu
#define SERIAL_8O2 0x3Eu
#endif

#define SERIAL_PORT_MONITOR Serial
#define SERIAL_PORT_HARDWARE Serial1
#define SERIAL_PORT_HARDWARE1 Serial1
#define SERIAL_PORT_HARDWARE_OPEN Serial1
#define SERIAL_PORT_USBVIRTUAL Serial
#define SERIAL_PORT_USBVIRTUAL_OPEN Serial
#define HAVE_HWSERIAL0 1
#define HAVE_HWSERIAL1 1
#define SerialUSB Serial
#define PSTR(string_literal) (string_literal)
#define FPSTR(string_pointer) (reinterpret_cast<const __FlashStringHelper *>(string_pointer))
#define F(string_literal) FPSTR(PSTR(string_literal))

#ifndef SDA
#define SDA PIN_WIRE_SDA
#define SCL PIN_WIRE_SCL
#define MISO PIN_SPI_MISO
#define MOSI PIN_SPI_MOSI
#define SCK PIN_SPI_SCK
#define SS NOT_A_PIN
#endif

#endif
