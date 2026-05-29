// NrfPeripherals.h - small bottom-level drivers for the nRF52840 peripherals
// that don't justify their own header pair:
//   * NrfRng   - hardware TRNG (RNG peripheral). True random, ~5 µs / byte.
//   * NrfWdt   - WDT watchdog. Up to 8 reload channels, no-pause-on-CPU-halt.
//   * NrfTemp  - internal die temperature sensor. ±2 °C, 0.25 °C resolution.
//   * NrfQdec  - quadrature decoder for rotary encoders. 24-bit accumulator,
//                 debounced sampling.
//   * NrfTimer - TIMER0..TIMER4. 32-bit / 16-bit timers with 6 compare
//                 channels each; Timer / Counter modes.
//   * NrfNvmc  - direct flash erase / write. Word-aligned, page-erased.
//   * NrfPpi   - Programmable Peripheral Interconnect: route a peripheral
//                 event to a peripheral task with no CPU in the path.
//
// Still missing (need their own dedicated driver / external hardware to
// verify): QSPI (external flash), PDM (MEMS mic), I2S, COMP / LPCOMP,
// EGU/SWI (software interrupt + event generator), MWU (memory watch).

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

// ---- NrfTimer - TIMER0..TIMER4 ---------------------------------------------
//
// Each TIMER has 6 capture/compare channels (CC0..CC5). Width:
//   TIMER0 / TIMER1 / TIMER2 - selectable 8 / 16 / 24 / 32 bit
//   TIMER3 / TIMER4          - 32-bit only
// Prescaler 0..9 divides the 16 MHz peripheral clock by 2^prescaler, so
// the counter clock is 16 MHz down to 31.25 kHz.
//
// CONFLICTS:
//   * TIMER0 is owned by NimBLE's controller and Zboss's nrf-802154 driver
//     (link-layer scheduler). If you're using either of those stacks pick
//     TIMER1..TIMER4 instead.
//   * TIMER4 is used by some Nordic LE Audio examples and the high-quality
//     SoftDevice variants. Safe on the verified promicroserialnosd path.
//
// All TIMER0-4 are otherwise free in this core.

typedef void (*nrfTimerCallback_t)(void);

class NrfTimer {
public:
    static constexpr uint8_t COMPARE_CHANNEL_COUNT = 6U;

    enum Index : uint8_t {
        TIMER0 = 0,   // shared with NimBLE / Zigbee controllers
        TIMER1 = 1,
        TIMER2 = 2,
        TIMER3 = 3,
        TIMER4 = 4,
    };

    explicit NrfTimer(uint8_t index);

    // Configure the timer to tick at approximately `tickHz`. Width: 32-bit
    // by default; pass `narrow16=true` to save power on TIMER0/1/2.
    bool begin(uint32_t tickHz, bool narrow16 = false);
    void end();

    void start();
    void stop();
    void clear();
    bool isRunning() const { return tickHz_ != 0UL; }
    bool isValid() const { return base_ != 0U; }

    uint32_t tickHz() const { return tickHz_; }
    uint32_t counter();   // CAPTURE[5] -> read CC[5]; reserved slot

    void setCompare(uint8_t cc, uint32_t value);
    uint32_t getCompare(uint8_t cc) const;

    void attachCompareInterrupt(uint8_t cc, nrfTimerCallback_t cb);
    void detachCompareInterrupt(uint8_t cc);

    // PPI-friendly: addresses of TASKS_CAPTURE[cc] and EVENTS_COMPARE[cc] so
    // PPI channels can wire them to other peripherals without CPU help.
    uint32_t taskCaptureAddr(uint8_t cc) const;
    uint32_t eventCompareAddr(uint8_t cc) const;
    uint32_t taskStartAddr() const;
    uint32_t taskStopAddr() const;

    // Dispatch hook used by the shared TIMERx_IRQHandler.
    void serviceIrq();

private:
    uint32_t base_;
    uint8_t  index_;
    uint8_t  irqNumber_;
    uint32_t tickHz_;
    nrfTimerCallback_t compareCallbacks_[COMPARE_CHANNEL_COUNT];
};

NrfTimer &nrfTimer0();
NrfTimer &nrfTimer1();
NrfTimer &nrfTimer2();
NrfTimer &nrfTimer3();
NrfTimer &nrfTimer4();

// ---- NrfNvmc - direct flash erase / write ---------------------------------
//
// nRF52840 flash: 1 MB, 256 pages of 4 KB each. Erase is page-aligned;
// write is word-aligned and only flips 1 bits to 0 (each word can be written
// once per erase cycle, multiple distinct partial writes ARE legal as long
// as you only clear bits never set).
//
// CAVEATS:
//   * Cannot execute code from the same flash region you're erasing - the
//     CPU stalls until the erase completes (~85 ms per page). Plan
//     erase/write for an idle window.
//   * Erasing or writing during a BLE / 802.15.4 / Thread session disrupts
//     timing because the flash controller takes the AHB bus exclusively.
//     The big-stack roadmaps document this; for now this driver is fine
//     for sketches that just want a "persist 100 bytes across reset" slot.
//   * No wear leveling. Each page is rated for ~10k erase cycles; if you
//     log frequently use the existing EEPROM library (which already
//     wear-levels) instead of this raw driver.
//
// Right answer for most sketches: keep using EEPROM for small persistent
// state. Use NrfNvmc only when you want to control your own flash region
// (e.g. a custom firmware partition or a calibration page).
class NrfNvmc {
public:
    static constexpr uint32_t PAGE_SIZE_BYTES = 4096U;

    static bool isReady();
    static bool erasePage(uint32_t flashAddress);
    static bool eraseRegion(uint32_t flashAddress, uint32_t lengthBytes);

    // Write one 32-bit word. The destination address must be word-aligned
    // and currently 0xFFFFFFFF. Returns false on alignment / busy / already-
    // written-bit-cleared.
    static bool writeWord(uint32_t flashAddress, uint32_t word);

    // Write a buffer (must be word-aligned start AND length).
    static bool writeWords(uint32_t flashAddress, const uint32_t *words, uint32_t wordCount);

    // Convenience wrapper for byte-aligned data: rounds up to next word.
    static bool writeBytes(uint32_t flashAddress, const uint8_t *bytes, size_t length);
};

// ---- NrfPpi - Programmable Peripheral Interconnect ------------------------
//
// 20 user-programmable channels (PPI 0..19) + 12 pre-programmed channels for
// fixed peripheral pairings. Each user channel wires:
//   one EVENT register address  ->  one TASK register address
// When the event fires, the task is triggered without CPU help.
//
// USE CASES:
//   * TIMER compare -> ADC sample
//   * GPIOTE input event -> TIMER capture (input timestamping)
//   * RTC compare -> NFCT activate (low-power tag wake)
//   * Any peripheral event -> GPIOTE output toggle (jitter-free signaling)
//
// The DPPI (Distributed PPI) variant on Cortex-M33 chips (nRF5340 / nRF9160)
// has a richer publish/subscribe model; this driver is the classic PPI.
class NrfPpi {
public:
    static constexpr uint8_t USER_CHANNEL_COUNT = 20U;
    static constexpr uint8_t INVALID_CHANNEL = 0xFFU;

    // Allocate the next free user PPI channel. Returns INVALID_CHANNEL when
    // all 20 are taken.
    static uint8_t allocateChannel();
    static void    releaseChannel(uint8_t channel);

    // Wire a channel: when the peripheral event at `eventAddr` fires, the
    // task at `taskAddr` is triggered. Use the helper getters on each
    // peripheral driver (e.g. NrfTimer::eventCompareAddr,
    // NrfTimer::taskStartAddr) to feed these.
    static bool configure(uint8_t channel, uint32_t eventAddr, uint32_t taskAddr);

    static void enable(uint8_t channel);
    static void disable(uint8_t channel);
    static bool isEnabled(uint8_t channel);

    // Convenience: allocate + configure + enable in one call. Returns the
    // allocated channel or INVALID_CHANNEL.
    static uint8_t wire(uint32_t eventAddr, uint32_t taskAddr);
};
