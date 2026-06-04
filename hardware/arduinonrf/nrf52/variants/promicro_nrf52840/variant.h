#pragma once

#include "../../cores/arduino/variant.h"

// ----------------------------------------------------------------------------
// Pin numbering follows the board's SILK-SCREEN: Arduino pin N == the pad
// labelled "Dn". So digitalWrite(6, ...) drives the pad marked "D6" (P1.00),
// Wire.begin() uses the "SDA"/"SCL" pads (D6/D7), analogRead(A0) reads the "A0"
// pad (D15 == P0.02), etc. The absolute nRF GPIO each pad maps to lives in
// g_ADigitalPinMap (variant.cpp).
//
//   Dn  nRF     silk         Dn  nRF     silk
//   D0  P0.06   TX           D11 P0.10   NFC2 *
//   D1  P0.08   RX           D12 P1.11
//   D2  P0.17   SCK          D13 P1.13   SDA1
//   D3  P0.20   MISO         D14 P1.15   SCL1
//   D4  P0.22   MOSI         D15 P0.02   A0
//   D5  P0.24   CS / SS      D16 P0.29   A1
//   D6  P1.00   SDA          D17 P0.31   A2
//   D7  P0.11   SCL          D18 P1.01   SCK1
//   D8  P1.04                D19 P1.02   MISO1
//   D9  P1.06                D20 P1.07   MOSI1
//   D10 P0.09   NFC1 *       --  P0.15   LED_BUILTIN (orange, active-high)
//
// * D10/D11 (P0.09/P0.10) are the NFC antenna pins; using them as GPIO needs the
//   UICR.NFCPINS=GPIO bit (configured automatically the first time you drive
//   them, which takes effect after the next reset).
// ----------------------------------------------------------------------------

// The single user / built-in LED is P0.15, ACTIVE-HIGH (verified on hardware via
// J-Link SWD). It is physically orange and is the same LED the bootloader blinks
// during DFU. It is not a "Dn" pad, so it lives at the end of the map (index 21).
static constexpr uint8_t LED_BUILTIN = 21;   // -> P0.15
static constexpr uint8_t LED_RED = 21;       // no RGB on this board - aliased
static constexpr uint8_t LED_GREEN = 21;
static constexpr uint8_t LED_BLUE = 21;

// Default UART (silk "TX"/"RX").
static constexpr uint8_t PIN_TX = 0;   // D0 -> P0.06
static constexpr uint8_t PIN_RX = 1;   // D1 -> P0.08

// Default SPI (silk "SCK"/"MISO"/"MOSI"/"CS").
static constexpr uint8_t SCK  = 2;   // D2 -> P0.17
static constexpr uint8_t MISO = 3;   // D3 -> P0.20
static constexpr uint8_t MOSI = 4;   // D4 -> P0.22
static constexpr uint8_t SS   = 5;   // D5 -> P0.24

// Default I2C (silk "SDA"/"SCL") - HARDWARE-VERIFIED 2026-06-03 with a GY-9250.
static constexpr uint8_t SDA = 6;   // D6 -> P1.00
static constexpr uint8_t SCL = 7;   // D7 -> P0.11

// Secondary buses (silk "SDA1"/"SCL1" and "SCK1"/"MISO1"/"MOSI1"). Wire1 and
// SPI1 share nRF peripheral instance 1, so only one of them can be active at a
// time (the core arbitrates this). The default Wire (instance 0) and SPI
// (instance 2) are independent and CAN run together.
static constexpr uint8_t SDA1  = 13;  // D13 -> P1.13  (Wire1 SDA)
static constexpr uint8_t SCL1  = 14;  // D14 -> P1.15  (Wire1 SCL)
static constexpr uint8_t SCK1  = 18;  // D18 -> P1.01  (SPI1 SCK)
static constexpr uint8_t MISO1 = 19;  // D19 -> P1.02  (SPI1 MISO)
static constexpr uint8_t MOSI1 = 20;  // D20 -> P1.07  (SPI1 MOSI)

// NFC antenna pins (silk "NFC1"/"NFC2"). Usable as plain GPIO D10/D11; the
// UICR.NFCPINS=GPIO bit is set automatically the first time you drive them
// (takes effect after the next reset).
static constexpr uint8_t NFC1 = 10;   // D10 -> P0.09
static constexpr uint8_t NFC2 = 11;   // D11 -> P0.10

// Analog (silk "A0"/"A1"/"A2"). Only three analog pads are broken out; A3..A7
// are not available on this board.
static constexpr uint8_t A0 = 15;     // D15 -> P0.02 (AIN0)
static constexpr uint8_t A1 = 16;     // D16 -> P0.29 (AIN5)
static constexpr uint8_t A2 = 17;     // D17 -> P0.31 (AIN7)
static constexpr uint8_t A3 = 0xFFU;
static constexpr uint8_t A4 = 0xFFU;
static constexpr uint8_t A5 = 0xFFU;
static constexpr uint8_t A6 = 0xFFU;
static constexpr uint8_t A7 = 0xFFU;
