#include "variant.h"

const NrfBoardCapabilities g_boardCapabilities = {
    "nice!nano v2",
    "promicro-compatible",
    "package-modeled",
    "package-modeled",
    "reference-core",
    "lfxo",
    true,
    true,
    true,
    true,
    true,
    false,
    true,
    true,
    true,
    false,
    false,
    false,
    false,
    false,
    5,
    1
};

const uint8_t PINS_COUNT = 23;
const uint8_t PIN_LED1 = LED_BUILTIN;
const uint8_t PIN_LED2 = 0xFFU;
const uint8_t PIN_LED3 = 0xFFU;
const uint8_t PIN_NEOPIXEL = 0xFFU;
const uint8_t PIN_BUTTON1 = 0xFFU;
const uint8_t PIN_BUTTON2 = 0xFFU;
const uint8_t PIN_BATTERY = 0xFFU;
const uint8_t PIN_EXT_VCC = EXT_VCC;
const uint8_t PIN_CHARGE_STATUS = 0xFFU;
const uint8_t PIN_QSPI_CS = 0xFFU;
const uint8_t PIN_QSPI_SCK = 0xFFU;
const uint8_t PIN_QSPI_IO0 = 0xFFU;
const uint8_t PIN_QSPI_IO1 = 0xFFU;
const uint8_t PIN_QSPI_IO2 = 0xFFU;
const uint8_t PIN_QSPI_IO3 = 0xFFU;
const uint8_t PIN_SERIAL_RX = D1;
const uint8_t PIN_SERIAL_TX = D0;
const uint8_t PIN_WIRE_SDA = SDA;
const uint8_t PIN_WIRE_SCL = SCL;
const uint8_t PIN_WIRE1_SDA = D13;
const uint8_t PIN_WIRE1_SCL = D14;
const uint8_t PIN_SPI_MISO = MISO;
const uint8_t PIN_SPI_MOSI = MOSI;
const uint8_t PIN_SPI_SCK = SCK;
const uint8_t PIN_SPI1_MISO = D19;
const uint8_t PIN_SPI1_MOSI = D20;
const uint8_t PIN_SPI1_SCK = D18;
const uint8_t PIN_A0 = A0;
const uint8_t PIN_A1 = A1;
const uint8_t PIN_A2 = A2;
const uint8_t PIN_A3 = 0xFFU;
const uint8_t PIN_A4 = 0xFFU;
const uint8_t PIN_A5 = 0xFFU;
const uint8_t PIN_A6 = 0xFFU;
const uint8_t PIN_A7 = 0xFFU;

NRF_VARIANT_STATIC_ASSERT_COMMON_PINS();

const uint32_t g_ADigitalPinMap[] = {
    6, 8, 17, 20, 22, 24, 32, 11, 36, 38, 9, 10, 43, 45, 47, 2, 29, 31, 33, 34, 39, 13, 15
};

NRF_VARIANT_STATIC_ASSERT_PINMAP_SIZE();
