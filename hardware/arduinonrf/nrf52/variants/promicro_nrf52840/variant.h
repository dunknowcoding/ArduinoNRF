#pragma once

#include "../../cores/arduino/variant.h"

// LED mapping HARDWARE-VERIFIED 2026-05-29 on the AliExpress ProMicro/SuperMini
// nRF52840 clone via J-Link SWD (drove each GPIO and watched the board):
//   * The single user / built-in LED is on P0.15 == Arduino pin 15, ACTIVE-HIGH
//     (digitalWrite(LED_BUILTIN, HIGH) lights it). It is physically ORANGE, and
//     is the SAME LED the bootloader blinks during DFU - there is no separate
//     "DFU LED".
//   * The other (blue) LED is the LiPo charger IC's status LED, wired to the
//     charger - NOT to any MCU GPIO (so PIN_CHARGE_STATUS stays 255). It blinks
//     ~2 Hz on USB power with no battery; firmware cannot control it.
//   * This board has NO RGB LED. LED_RED/GREEN/BLUE below are framework
//     placeholders (PIN_LED2/3 are static-asserted + feed hasRgbLed), so they
//     are aliased to the one real LED rather than pointing at dead pins.
// The previous LED_BUILTIN=13 was an unverified package-model guess and also
// collided with SCK=13.
static constexpr uint8_t LED_BUILTIN = 15;   // P0.15, active-high (verified)
static constexpr uint8_t LED_RED = 15;       // no RGB on this board - alias to the one LED
static constexpr uint8_t LED_GREEN = 15;
static constexpr uint8_t LED_BLUE = 15;
static constexpr uint8_t SDA = 18;
static constexpr uint8_t SCL = 19;
static constexpr uint8_t SS = 10;
static constexpr uint8_t MOSI = 11;
static constexpr uint8_t MISO = 12;
static constexpr uint8_t SCK = 13;
static constexpr uint8_t A0 = 20;
static constexpr uint8_t A1 = 21;
static constexpr uint8_t A2 = 22;
static constexpr uint8_t A3 = 23;
static constexpr uint8_t A4 = 24;
static constexpr uint8_t A5 = 25;
static constexpr uint8_t A6 = 26;
static constexpr uint8_t A7 = 27;