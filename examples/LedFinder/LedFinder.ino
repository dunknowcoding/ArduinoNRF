// LedFinder.ino - find the real user/built-in LED pin on an unverified board.
//
// Toggles ONE GPIO at a time at 0.5 Hz (1 s on / 1 s off) - a rate clearly
// distinct from this board's ~2 Hz charger LED. The pin under test is the
// ABSOLUTE nRF52840 pin number in g_target (0..31 = P0.00..P0.31,
// 32..47 = P1.00..P1.15), which is WRITABLE over SWD while running, so a host
// can sweep pins without reflashing. Driven by raw registers (not the Arduino
// API) so every pin is reachable - the board's g_ADigitalPinMap only covers
// P0.00..P0.29. Toggling (not static) makes the LED blink regardless of
// active-high vs active-low wiring.
//
// Skips P0.00/P0.01 (LFXO 32 kHz crystal) and P0.18 (nRESET) to avoid
// disrupting the clock / resetting the chip.

#include <Arduino.h>

// SWD-writable: set this to the absolute pin to test. SWD-readable mirror in
// g_current so the host can confirm what's being driven.
__attribute__((used)) volatile uint32_t g_target  = 15;   // start at P0.15 (nice!nano user LED)
__attribute__((used)) volatile uint32_t g_current = 0xFFFFFFFFUL;

static inline volatile uint32_t* pincnf(uint32_t absPin) {
  uint32_t base = (absPin < 32U) ? 0x50000000UL : 0x50000300UL;
  return (volatile uint32_t*)(base + 0x700UL + (absPin & 31U) * 4UL);
}
static inline volatile uint32_t* outset(uint32_t absPin) {
  uint32_t base = (absPin < 32U) ? 0x50000000UL : 0x50000300UL;
  return (volatile uint32_t*)(base + 0x508UL);
}
static inline volatile uint32_t* outclr(uint32_t absPin) {
  uint32_t base = (absPin < 32U) ? 0x50000000UL : 0x50000300UL;
  return (volatile uint32_t*)(base + 0x50CUL);
}

static bool unsafe(uint32_t p) { return (p == 0U || p == 1U || p == 18U || p > 47U); }

static void releasePin(uint32_t absPin) {
  if (unsafe(absPin)) return;
  *pincnf(absPin) = 0x00000002UL;   // DIR=input, input buffer disconnected (reset default)
}

void setup() {
  SerialService.begin(115200);
}

void loop() {
  static uint32_t prev = 0xFFFFFFFFUL;
  static bool state = false;

  uint32_t t = g_target;
  if (t != prev) {
    if (prev != 0xFFFFFFFFUL) releasePin(prev);
    prev = t;
    state = false;
  }
  g_current = t;

  if (!unsafe(t)) {
    *pincnf(t) = 0x00000001UL;                 // DIR=output, push-pull
    if (state) *outset(t) = (1UL << (t & 31U));
    else       *outclr(t) = (1UL << (t & 31U));
  }
  state = !state;

  SerialService.print("testing abs pin ");
  SerialService.print(t);
  SerialService.println(t < 32 ? " (P0)" : " (P1)");
  delay(1000);   // 0.5 Hz toggle
}
