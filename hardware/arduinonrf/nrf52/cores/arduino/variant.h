#pragma once

#include <stdint.h>

struct NrfBoardCapabilities {
    const char *boardName;
    const char *boardFamily;
    const char *batteryModelEvidenceLevel;
    const char *uploadProfileEvidenceLevel;
    const char *clockSourceEvidenceLevel;
    const char *lowFrequencyClockSource;
    bool hasNativeUsb;
    bool hasBatterySense;
    bool hasQspiFlash;
    bool hasRgbLed;
    bool hasWiFiCoprocessor;
    bool hasImu;
    bool hasNfc;
    bool batterySenseViaVddhDiv5;
    bool pinMapVerified;
    bool batteryModelVerified;
    bool uploadProfileVerified;
    bool lowPowerProfileModeled;
    bool spi1PinsVerified;
    bool wire1PinsVerified;
    uint8_t batteryVoltageScaleNumerator;
    uint8_t batteryVoltageScaleDenominator;
};

extern const NrfBoardCapabilities g_boardCapabilities;
extern const uint32_t g_ADigitalPinMap[];

extern const uint8_t PIN_LED1;
extern const uint8_t PIN_LED2;
extern const uint8_t PIN_LED3;
extern const uint8_t PIN_NEOPIXEL;
extern const uint8_t PIN_BUTTON1;
extern const uint8_t PIN_BUTTON2;
extern const uint8_t PIN_BATTERY;
extern const uint8_t PIN_EXT_VCC;
extern const uint8_t PIN_CHARGE_STATUS;
extern const uint8_t PIN_QSPI_CS;
extern const uint8_t PIN_QSPI_SCK;
extern const uint8_t PIN_QSPI_IO0;
extern const uint8_t PIN_QSPI_IO1;
extern const uint8_t PIN_QSPI_IO2;
extern const uint8_t PIN_QSPI_IO3;

extern const uint8_t PIN_SERIAL_RX;
extern const uint8_t PIN_SERIAL_TX;
extern const uint8_t PIN_WIRE_SDA;
extern const uint8_t PIN_WIRE_SCL;
extern const uint8_t PIN_WIRE1_SDA;
extern const uint8_t PIN_WIRE1_SCL;
extern const uint8_t PIN_SPI_MISO;
extern const uint8_t PIN_SPI_MOSI;
extern const uint8_t PIN_SPI_SCK;
extern const uint8_t PIN_SPI_SS;
extern const uint8_t PIN_SPI1_MISO;
extern const uint8_t PIN_SPI1_MOSI;
extern const uint8_t PIN_SPI1_SCK;
extern const uint8_t PIN_A0;
extern const uint8_t PIN_A1;
extern const uint8_t PIN_A2;
extern const uint8_t PIN_A3;
extern const uint8_t PIN_A4;
extern const uint8_t PIN_A5;
extern const uint8_t PIN_A6;
extern const uint8_t PIN_A7;
extern const uint8_t PINS_COUNT;

constexpr bool nrfVariantPinDeclared(uint32_t pin, uint32_t pinCount) {
    return pin == 0xFFU || pin < pinCount;
}

#define NRF_VARIANT_STATIC_ASSERT_PIN(pinName) \
    static_assert(nrfVariantPinDeclared((pinName), PINS_COUNT), #pinName " must be within PINS_COUNT or 0xFF")

#define NRF_VARIANT_STATIC_ASSERT_COMMON_PINS() \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_LED1); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_LED2); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_LED3); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_NEOPIXEL); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_BUTTON1); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_BUTTON2); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_BATTERY); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_EXT_VCC); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_CHARGE_STATUS); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_QSPI_CS); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_QSPI_SCK); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_QSPI_IO0); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_QSPI_IO1); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_QSPI_IO2); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_QSPI_IO3); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_SERIAL_RX); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_SERIAL_TX); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_WIRE_SDA); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_WIRE_SCL); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_WIRE1_SDA); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_WIRE1_SCL); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_SPI_MISO); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_SPI_MOSI); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_SPI_SCK); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_SPI_SS); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_SPI1_MISO); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_SPI1_MOSI); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_SPI1_SCK); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_A0); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_A1); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_A2); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_A3); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_A4); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_A5); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_A6); \
    NRF_VARIANT_STATIC_ASSERT_PIN(PIN_A7)

#define NRF_VARIANT_STATIC_ASSERT_PINMAP_SIZE() \
    static_assert((sizeof(g_ADigitalPinMap) / sizeof(g_ADigitalPinMap[0])) == PINS_COUNT, "g_ADigitalPinMap size must match PINS_COUNT")
