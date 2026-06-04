#include "variant.h"

const NrfBoardCapabilities g_boardCapabilities = {
    "AliExpress ProMicro nRF52840", "promicro-compatible", "package-modeled", "package-modeled", "schematic", "lfxo",
    true,   // hasNativeUsb
    true,   // hasBatterySense
    false,  // hasQspiFlash
    false,  // hasRgbLed
    false,  // hasWiFiCoprocessor
    false,  // hasImu
    false,  // hasNfc (P0.09/P0.10 are exposed as D10/D11 GPIO by default)
    false,  // batterySenseViaVddhDiv5
    true,   // pinMapVerified (numbering matches the board silk-screen)
    false,  // batteryModelVerified
    false,  // uploadProfileVerified
    false,  // lowPowerProfileModeled
    false,  // spi1PinsVerified (silk-modeled, not individually HW-tested)
    false,  // wire1PinsVerified (silk-modeled, not individually HW-tested)
    1, 1
};

const uint8_t PINS_COUNT = 23;
const uint8_t PIN_LED1 = LED_BUILTIN;
const uint8_t PIN_LED2 = LED_RED;
const uint8_t PIN_LED3 = LED_GREEN;
const uint8_t PIN_NEOPIXEL = 0xFFU;
const uint8_t PIN_BUTTON1 = 0xFFU;
const uint8_t PIN_BUTTON2 = 0xFFU;
const uint8_t PIN_BATTERY = A1;          // VBAT divider modeled on P0.29 (D16/A1)
const uint8_t PIN_EXT_VCC = 22;          // P0.13 (silk "EXT_VCC")
const uint8_t PIN_CHARGE_STATUS = 255;
const uint8_t PIN_QSPI_CS = 255;
const uint8_t PIN_QSPI_SCK = 255;
const uint8_t PIN_QSPI_IO0 = 255;
const uint8_t PIN_QSPI_IO1 = 255;
const uint8_t PIN_QSPI_IO2 = 255;
const uint8_t PIN_QSPI_IO3 = 255;
const uint8_t PIN_SERIAL_TX = PIN_TX;    // D0 -> P0.06
const uint8_t PIN_SERIAL_RX = PIN_RX;    // D1 -> P0.08
const uint8_t PIN_WIRE_SDA = SDA;        // D6 -> P1.00
const uint8_t PIN_WIRE_SCL = SCL;        // D7 -> P0.11
const uint8_t PIN_WIRE1_SDA = SDA1;      // D13 -> P1.13
const uint8_t PIN_WIRE1_SCL = SCL1;      // D14 -> P1.15
const uint8_t PIN_SPI_MISO = MISO;       // D3 -> P0.20
const uint8_t PIN_SPI_MOSI = MOSI;       // D4 -> P0.22
const uint8_t PIN_SPI_SCK = SCK;         // D2 -> P0.17
const uint8_t PIN_SPI_SS = SS;
const uint8_t PIN_SPI1_MISO = MISO1;     // D19 -> P1.02
const uint8_t PIN_SPI1_MOSI = MOSI1;     // D20 -> P1.07
const uint8_t PIN_SPI1_SCK = SCK1;       // D18 -> P1.01
const uint8_t PIN_A0 = A0;
const uint8_t PIN_A1 = A1;
const uint8_t PIN_A2 = A2;
const uint8_t PIN_A3 = A3;
const uint8_t PIN_A4 = A4;
const uint8_t PIN_A5 = A5;
const uint8_t PIN_A6 = A6;
const uint8_t PIN_A7 = A7;

NRF_VARIANT_STATIC_ASSERT_COMMON_PINS();

// Index == silk "Dn"; value == absolute nRF GPIO (P0.xx == xx, P1.xx == 32+xx).
const uint32_t g_ADigitalPinMap[] = {
    6,   //  D0  P0.06  TX
    8,   //  D1  P0.08  RX
    17,  //  D2  P0.17  SCK
    20,  //  D3  P0.20  MISO
    22,  //  D4  P0.22  MOSI
    24,  //  D5  P0.24  CS / SS
    32,  //  D6  P1.00  SDA
    11,  //  D7  P0.11  SCL
    36,  //  D8  P1.04
    38,  //  D9  P1.06
    9,   //  D10 P0.09  NFC1
    10,  //  D11 P0.10  NFC2
    43,  //  D12 P1.11
    45,  //  D13 P1.13  SDA1
    47,  //  D14 P1.15  SCL1
    2,   //  D15 P0.02  A0  (AIN0)
    29,  //  D16 P0.29  A1  (AIN5)
    31,  //  D17 P0.31  A2  (AIN7)
    33,  //  D18 P1.01  SCK1
    34,  //  D19 P1.02  MISO1
    39,  //  D20 P1.07  MOSI1
    15,  //  21  P0.15  LED_BUILTIN (orange, active-high) - not a Dn pad
    13   //  22  P0.13  EXT_VCC                            - not a Dn pad
};

NRF_VARIANT_STATIC_ASSERT_PINMAP_SIZE();
