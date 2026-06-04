#include "variant.h"

const NrfBoardCapabilities g_boardCapabilities = {
    "Mini nRF52840", "mini-module", "package-modeled", "package-modeled", "undeclared", "undeclared", true, true, true, true, false, false, false, false, false, false, false, false, false, 1, 1
};

const uint8_t PINS_COUNT = 32;
const uint8_t PIN_LED1 = LED_BUILTIN;
const uint8_t PIN_LED2 = LED_RED;
const uint8_t PIN_LED3 = LED_GREEN;
const uint8_t PIN_NEOPIXEL = LED_BLUE;
const uint8_t PIN_BUTTON1 = 13;
const uint8_t PIN_BUTTON2 = 12;
const uint8_t PIN_BATTERY = 31;
const uint8_t PIN_EXT_VCC = 255;
const uint8_t PIN_CHARGE_STATUS = 255;
const uint8_t PIN_QSPI_CS = 21;
const uint8_t PIN_QSPI_SCK = 22;
const uint8_t PIN_QSPI_IO0 = 23;
const uint8_t PIN_QSPI_IO1 = 28;
const uint8_t PIN_QSPI_IO2 = 29;
const uint8_t PIN_QSPI_IO3 = 30;
const uint8_t PIN_SERIAL_RX = 0;
const uint8_t PIN_SERIAL_TX = 1;
const uint8_t PIN_WIRE_SDA = SDA;
const uint8_t PIN_WIRE_SCL = SCL;
const uint8_t PIN_WIRE1_SDA = 0xFFU;
const uint8_t PIN_WIRE1_SCL = 0xFFU;
const uint8_t PIN_SPI_MISO = MISO;
const uint8_t PIN_SPI_MOSI = MOSI;
const uint8_t PIN_SPI_SCK = SCK;
const uint8_t PIN_SPI_SS = SS;
const uint8_t PIN_SPI1_MISO = 0xFFU;
const uint8_t PIN_SPI1_MOSI = 0xFFU;
const uint8_t PIN_SPI1_SCK = 0xFFU;
const uint8_t PIN_A0 = A0;
const uint8_t PIN_A1 = A1;
const uint8_t PIN_A2 = A2;
const uint8_t PIN_A3 = A3;
const uint8_t PIN_A4 = A4;
const uint8_t PIN_A5 = A5;
const uint8_t PIN_A6 = A6;
const uint8_t PIN_A7 = A7;

NRF_VARIANT_STATIC_ASSERT_COMMON_PINS();

const uint32_t g_ADigitalPinMap[] = {
    0, 1, 2, 3, 4, 5, 6, 7,
    8, 9, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23,
    24, 25, 26, 27, 28, 29, 30, 31
};

NRF_VARIANT_STATIC_ASSERT_PINMAP_SIZE();