// NrfRtc.h - bottom-level driver for the nRF52 Real-Time Counters (RTC0/1/2).
//
// Each RTC is a 24-bit up-counter clocked from LFCLK (32.768 kHz) through a
// 12-bit prescaler, with 4 independent compare registers (CC0..CC3). They are
// the right peripheral for low-power, drift-free timekeeping while the MCU
// sleeps - much cheaper to run than TIMERx (which uses HFCLK) and the
// peripheral keeps running through System ON sleep.
//
// IMPORTANT - peripheral conflicts:
//   * RTC0 is reserved by the SoftDevice on the with-SD build profiles.
//     Don't use it on those targets. The verified promicroserialnosd path
//     leaves all three RTCs free.
//   * RTC1 is a popular choice for an RTOS tick on Adafruit/Mbed cores;
//     this Arduino core does not touch it.
//   * RTC2 is normally idle.
//
// Typical use:
//   NrfRtc rtc(2);                                  // RTC2
//   rtc.begin(1000);                                // 1 kHz tick (~1 ms)
//   rtc.attachCompareInterrupt(0, onTick);          // CC0 -> ISR
//   rtc.setCompare(0, rtc.counter() + 1000);        // fire in ~1 s
//   rtc.start();
//
// The driver is intentionally thin (one volatile per call) so it composes
// cleanly with NimBLE / lp_ticker style timekeeping.

#pragma once

#include <stdint.h>

typedef void (*nrfRtcCallback_t)(void);

class NrfRtc {
public:
    static constexpr uint8_t COMPARE_CHANNEL_COUNT = 4U;

    // index = 0..2 selecting RTC0 / RTC1 / RTC2. Out-of-range constructs an
    // invalid driver that returns false / 0 from every call.
    explicit NrfRtc(uint8_t index);

    // Configure prescaler so the counter ticks at approximately `tickHz`.
    // tickHz must be in [8, 32768] (the LFCLK rate / prescaler range). Starts
    // LFCLK if not already running. Returns false on invalid index / range.
    bool begin(uint32_t tickHz);

    // Stop the RTC, disable its interrupts, and detach handlers. Safe to call
    // multiple times. Does not stop LFCLK (other peripherals may be using it).
    void end();

    void start();
    void stop();

    // Reset the counter to zero. Take care: any in-flight compare scheduled
    // off `counter() + N` will need to be re-armed.
    void clear();

    bool isRunning() const;
    bool isValid() const { return base_ != 0U; }

    uint32_t counter() const;            // 24-bit raw value
    uint32_t prescaler() const;
    uint32_t tickHz() const { return tickHz_; }
    uint32_t periodUs() const;           // microseconds per tick (rounded)

    // Set a compare register. Fires the compare event/IRQ when the counter
    // matches. ccIndex 0..3.
    void setCompare(uint8_t ccIndex, uint32_t ticks);
    uint32_t getCompare(uint8_t ccIndex) const;

    // Hook the compare match or counter overflow (24-bit wrap) to a callback.
    // The callback runs in interrupt context - keep it short.
    void attachCompareInterrupt(uint8_t ccIndex, nrfRtcCallback_t callback);
    void detachCompareInterrupt(uint8_t ccIndex);
    void attachOverflowInterrupt(nrfRtcCallback_t callback);
    void detachOverflowInterrupt();

    // Helper: arm CC0 to fire in `delayMs` milliseconds from now (clamped to
    // 24-bit range at the current tickHz). Returns the scheduled tick value.
    uint32_t scheduleInMs(uint32_t delayMs);

    // Dispatch hook called from the shared RTC IRQ handler (defined in
    // NrfRtc.cpp). Public so the global handlers can reach it; not for users.
    void serviceIrq();

private:
    uint32_t base_;
    uint8_t  index_;
    uint8_t  irqNumber_;
    uint32_t tickHz_;
    nrfRtcCallback_t compareCallbacks_[COMPARE_CHANNEL_COUNT];
    nrfRtcCallback_t overflowCallback_;
};

// Get a process-wide singleton for each RTC instance. NrfRtc itself is cheap
// to construct stack-side, but these singletons are what the IRQ handlers
// dispatch to so user-instantiated objects can't race the ISR.
NrfRtc &nrfRtc0();
NrfRtc &nrfRtc1();
NrfRtc &nrfRtc2();
