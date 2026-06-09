# RTC driver — low-level access to RTC0/1/2

The nRF52 has three Real-Time Counters (RTC0, RTC1, RTC2). Each is a **24-bit up-counter clocked from LFCLK (32.768 kHz)** through a 12-bit prescaler, with **4 independent compare channels**. They are the right peripheral for:

- low-power timekeeping while the MCU sleeps (LFCLK survives System ON sleep; HFCLK TIMERx peripherals do not),
- drift-free scheduling without leaning on the single SysTick,
- precise alarm callbacks (4 compares × 3 RTCs = 12 hardware alarm slots).

Before this addition the core didn't expose them at all.

## Peripheral-claim warnings (read first)

- **RTC0** is reserved by the Nordic SoftDevice on with-SD build profiles. No-SoftDevice bootloader menu entries leave all three RTCs free.
- **RTC1** is a popular choice for an RTOS tick on other cores (Mbed, Adafruit). This core does not use it.
- **RTC2** is normally idle and the safe default for new sketches.

Picking the wrong RTC won't corrupt anything immediately, but the IRQ may collide with another consumer. When in doubt, pick `RTC2`.

## API

```cpp
#include <NrfRtc.h>

class NrfRtc {
public:
    static constexpr uint8_t COMPARE_CHANNEL_COUNT = 4U;
    explicit NrfRtc(uint8_t index);             // 0..2 selects RTC0/1/2

    bool begin(uint32_t tickHz);                // pick prescaler for ~tickHz; starts LFCLK
    void end();
    void start();
    void stop();
    void clear();                               // counter -> 0

    uint32_t counter() const;                   // 24-bit raw counter
    uint32_t prescaler() const;
    uint32_t tickHz() const;
    uint32_t periodUs() const;                  // microseconds per tick

    void setCompare(uint8_t cc, uint32_t ticks);
    uint32_t getCompare(uint8_t cc) const;

    void attachCompareInterrupt(uint8_t cc, nrfRtcCallback_t cb);
    void detachCompareInterrupt(uint8_t cc);
    void attachOverflowInterrupt(nrfRtcCallback_t cb);
    void detachOverflowInterrupt();

    uint32_t scheduleInMs(uint32_t delayMs);    // arm CC0 for delayMs from now
};

NrfRtc &nrfRtc0();
NrfRtc &nrfRtc1();
NrfRtc &nrfRtc2();    // recommended default
```

Callbacks run in ISR context — keep them short, re-arm `CC` if you need a periodic tick, and avoid blocking calls.

## How it picks the prescaler

`begin(tickHz)` chooses the 12-bit prescaler that minimises `LFCLK / (1 + prescaler) - tickHz`. The achievable rate range is **8 Hz … 32.768 kHz**. The actual configured rate is returned by `tickHz()` after `begin()`. Common picks:

| Requested | Configured | Prescaler | Period |
|---|---|---|---|
| 32768 Hz | 32768 Hz | 0 | 30.5 µs |
| 1000 Hz | 1024 Hz | 31 | 977 µs |
| 100 Hz | ~102.4 Hz | 319 | 9.77 ms |
| 8 Hz | 8 Hz | 4095 | 125 ms |

## Example

`examples/RtcAlarm/RtcAlarm.ino`: RTC2 at 1 kHz, CC0 fires every 500 ms via an ISR that toggles `LED_BUILTIN` and increments a tick counter the main loop reports over Serial.

```cpp
NrfRtc &rtc = nrfRtc2();
rtc.begin(1000U);
rtc.attachCompareInterrupt(0, onAlarm);
rtc.setCompare(0, rtc.counter() + 500U);
rtc.start();
```

## Implementation notes

- Singleton accessors (`nrfRtc0/1/2`) are what the global IRQ handlers dispatch to, so a user-stacked `NrfRtc` going out of scope can't deliver interrupts. Always operate on the singleton when you need IRQs.
- `startup_nrf52.cpp` declares `RTC0_IRQHandler` / `RTC1_IRQHandler` / `RTC2_IRQHandler` with weak aliases to `Default_Handler`, then points the vector table at them. `NrfRtc.cpp`'s non-weak definitions override the weak default, so the ISRs reach `serviceIrq()`.
- The driver does **not** start LFCLK unconditionally — `begin()` only kicks the crystal/RC start if `nrfLfclkRunning()` is false, so multiple RTCs share one LFCLK init cleanly.
