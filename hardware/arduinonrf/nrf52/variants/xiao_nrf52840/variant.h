#pragma once

#include "../../cores/arduino/variant.h"

// Seeed XIAO nRF52840 3-in-1 RGB user LED. It is COMMON-ANODE => ACTIVE LOW:
// drive the pin LOW to light the colour. Pins per the Seeed schematic
// (LED_RED P0.26, LED_BLUE P0.06, LED_GREEN P0.30).
static constexpr uint8_t LED_BUILTIN = 11;  // = LED_RED  -> P0.26
static constexpr uint8_t LED_RED = 11;      // -> P0.26
static constexpr uint8_t LED_BLUE = 12;     // -> P0.06
static constexpr uint8_t LED_GREEN = 13;    // -> P0.30
static constexpr uint8_t SDA = 4;
static constexpr uint8_t SCL = 5;
static constexpr uint8_t SS = 7;
static constexpr uint8_t MOSI = 9;
static constexpr uint8_t MISO = 10;
static constexpr uint8_t SCK = 8;
// Analog inputs A0..A5 are the same pads as D0..D5 (all ADC-capable on XIAO).
static constexpr uint8_t A0 = 0;   // P0.02 (AIN0)
static constexpr uint8_t A1 = 1;   // P0.03 (AIN1)
static constexpr uint8_t A2 = 2;   // P0.28 (AIN4)
static constexpr uint8_t A3 = 3;   // P0.29 (AIN5)
static constexpr uint8_t A4 = 4;   // P0.04 (AIN2) - shared with D4/SDA
static constexpr uint8_t A5 = 5;   // P0.05 (AIN3) - shared with D5/SCL
static constexpr uint8_t A6 = 30;  // internal: P0.25 (no silk pad)
static constexpr uint8_t A7 = 31;  // internal: P0.26 (no silk pad)