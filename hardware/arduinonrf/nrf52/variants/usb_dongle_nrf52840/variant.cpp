#include "variant.h"

const NrfBoardCapabilities g_boardCapabilities = {
    "nRF52840 USB Dongle", "usb-dongle", "not-applicable", "package-modeled", "undeclared", "undeclared", true, false, false, true, false, false, true, true, true, false, false, false, false, 1, 1
};

const uint8_t PINS_COUNT = 21;
const uint8_t PIN_LED1 = LED_BUILTIN;
const uint8_t PIN_LED2 = LED_GREEN;
const uint8_t PIN_LED3 = LED_BLUE;
const uint8_t PIN_NEOPIXEL = 255;
const uint8_t PIN_BUTTON1 = 20;   // SW1 -> P1.06 (active low)
const uint8_t PIN_BUTTON2 = 255;
const uint8_t PIN_BATTERY = 255;
const uint8_t PIN_EXT_VCC = 255;
const uint8_t PIN_CHARGE_STATUS = 255;
const uint8_t PIN_QSPI_CS = 255;
const uint8_t PIN_QSPI_SCK = 255;
const uint8_t PIN_QSPI_IO0 = 255;
const uint8_t PIN_QSPI_IO1 = 255;
const uint8_t PIN_QSPI_IO2 = 255;
const uint8_t PIN_QSPI_IO3 = 255;
const uint8_t PIN_SERIAL_RX = 13;
const uint8_t PIN_SERIAL_TX = 14;
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
    0, 1, 2, 3, 4, 5, 6, 7, 8,   // 0..18 = P0.00..P0.18 (identity)
    9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
    41,  // 19 = P1.09 (LD2 green)
    38   // 20 = P1.06 (SW1 button)
};

NRF_VARIANT_STATIC_ASSERT_PINMAP_SIZE();