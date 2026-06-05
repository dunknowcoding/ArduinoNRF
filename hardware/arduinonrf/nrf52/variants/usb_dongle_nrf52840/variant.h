#pragma once

#include "../../cores/arduino/variant.h"

// PCA10059 LEDs are ACTIVE LOW (drive LOW to light). LD1 is a single green
// LED; LD2 is an RGB LED. Pins per Nordic's PCA10059 User Guide.
static constexpr uint8_t LED_BUILTIN = 6;   // LD1 green -> P0.06
static constexpr uint8_t LED_RED = 8;       // LD2 red   -> P0.08
static constexpr uint8_t LED_GREEN = 19;    // LD2 green -> P1.09
static constexpr uint8_t LED_BLUE = 12;     // LD2 blue  -> P0.12
static constexpr uint8_t SDA = 13;
static constexpr uint8_t SCL = 14;
static constexpr uint8_t SS = 15;
static constexpr uint8_t MOSI = 16;
static constexpr uint8_t MISO = 17;
static constexpr uint8_t SCK = 18;
static constexpr uint8_t A0 = 9;
static constexpr uint8_t A1 = 10;
static constexpr uint8_t A2 = 11;
static constexpr uint8_t A3 = 12;
static constexpr uint8_t A4 = 13;
static constexpr uint8_t A5 = 14;
static constexpr uint8_t A6 = 15;
static constexpr uint8_t A7 = 16;