// NrfPeripherals.h - small bottom-level drivers for the nRF52840 peripherals
// that don't justify their own header pair:
//   * NrfRng  - hardware TRNG (RNG peripheral). True random, ~5 µs / byte.
//   * NrfWdt  - WDT watchdog. Up to 8 reload channels, no-pause-on-CPU-halt.
//   * NrfTemp - internal die temperature sensor. ±2 °C, 0.25 °C resolution.
//   * NrfQdec - quadrature decoder for rotary encoders. 24-bit accumulator,
//                debounced sampling.
//
// These are all SMALL drivers; the full nRF52840 peripheral list also
// includes TIMER0-4, QSPI, PDM, I2S, COMP, LPCOMP, EGU, SWI, MWU and PPI
// that each need their own dedicated driver - see the integration roadmap
// docs for the heavyweight stacks (NimBLE, Zigbee, Thread, CC310).

#pragma once

#include <stdint.h>
#include <stddef.h>

// ---- NrfRng - hardware TRNG ------------------------------------------------
//
// Each call to randomBytes() bit-bangs the RNG peripheral with the bias
// corrector enabled (DERCEN=1) and returns whitened random bytes. The
// peripheral takes ~5 µs per byte with the corrector on. NrfRng does NOT
// claim an IRQ - it busy-polls EVENTS_VALRDY to keep the API synchronous.
//
// nRF52840 has a separate hardware path on CC310 that's faster (~1 µs / byte)
// once that integration lands. The CC310 path will live in libraries/CC310/
// (see CC310 roadmap).
class NrfRng {
public:
    static void begin();          // enable bias corrector + start
    static void end();
    static bool isRunning();

    // Fill `buf` with `len` random bytes. Blocks until done. No error path -
    // hardware TRNG always succeeds on a healthy chip.
    static void randomBytes(uint8_t *buf, size_t len);

    // Convenience: single uint32_t in one call.
    static uint32_t random32();
};

// ---- NrfWdt - watchdog timer ----------------------------------------------
//
// nRF52840's WDT runs off LFCLK (default RC, can use external XO). After
// start() it cannot be stopped except by reset. Multiple reload registers
// (RR[0..7]) can be enabled; each must be fed within the timeout window or
// the chip resets.
//
// CAVEAT: enabling WDT in a debug session is annoying because the watchdog
// keeps ticking while the CPU is halted in the GDB stub. The header includes
// a HALT_PAUSE option that uses the PAUSE register (set the BEHAVIOUR field)
// so the WDT pauses while halted.
class NrfWdt {
public:
    static constexpr uint8_t MAX_RELOAD_CHANNELS = 8U;

    // Configure timeout in milliseconds (granularity 1 ms; resolution ~30.5
    // us limited by LFCLK). pauseOnHalt: pause WDT while debugger has the
    // CPU halted (recommended on for development).
    // Channels enabled in `channelMask` will be checked. Bit 0 = RR[0], etc.
    static bool begin(uint32_t timeoutMs, uint8_t channelMask = 1U,
                       bool pauseOnHalt = true);

    // Feed the watchdog for a given channel (0..7).
    static void feed(uint8_t channel = 0);

    static bool isRunning();
    // Did our last boot come from a watchdog reset?
    static bool causedLastReset();
};

// ---- NrfTemp - internal die temperature sensor -----------------------------
//
// Reading is one-shot: start, wait for done (typ. 20 µs), read result. The
// result is in units of 0.25 °C as an int32. Convenience methods convert.
class NrfTemp {
public:
    // Acquire a single reading. Returns true on success, false on timeout.
    static bool readRaw(int32_t *outQuartersC);

    static float    readCelsius();      // returns NAN on timeout
    static float    readFahrenheit();
};

// ---- NrfQdec - quadrature decoder ----------------------------------------
//
// Hardware A/B decoder. Two pins (A, B) wired to an encoder; an optional LED
// pin for active illumination. The peripheral samples at a configurable rate
// and accumulates +1 / -1 / +2 / -2 etc into a 16-bit SAMPLE register and a
// signed 32-bit ACC.
class NrfQdec {
public:
    // sampleRateHz: how often the decoder samples the inputs.
    // 8000 / 4000 / 2000 / 1000 / 500 / 250 / 125 / 64 / 32 / 16 Hz are the
    // supported choices; pass 0 to use the default 1024 Hz.
    static bool begin(uint8_t pinA, uint8_t pinB, uint8_t pinLed = 0xFFU,
                       uint16_t sampleRateHz = 0);
    static void end();

    // Read accumulator (signed clicks since last read). After reading, ACC
    // is cleared so the next call returns ONLY new clicks.
    static int32_t readDelta();

    // Read current running total without clearing.
    static int32_t readPosition();
    static void    resetPosition();

    static bool isRunning();
};
