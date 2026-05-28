# PWM — multi-module / 16-channel facade

This core now drives all four nRF52840 PWM peripherals (PWM0–PWM3). Each module is one independent frequency group with up to 4 channels, so up to **16 PWM channels** can run simultaneously, in **4 independent frequency groups**. The capability surface (`nrfPwmTimerGroupCount() == 4`, `nrfPwmIndependentTimersSupported() == true`, `nrfPwmChannelCapacity() == 16`) reflects this.

## Allocation model

Three knobs decide which module + channel a pin lands on:

| Function | Behavior |
|---|---|
| `analogWrite(pin, value)` *(legacy Arduino)* | Calls `assignPwmSlot(pin)` which **consolidates** — prefers an already-active module with a free channel. All pins end up sharing one group's frequency. Backward-compatible with single-PWM behavior. |
| `nrfPwmSetPinFrequency(pin, hz)` | **Frequency-aware**. First tries to join an existing module already running at *that* frequency (true group sharing); if none, claims a fresh idle module so the pin gets its own independent group. Returns false if all 4 modules are at incompatible frequencies. |
| `nrfPwmConfigureComplementary(pinA, pinB, deadTimeTicks)` | Forces both pins onto the **same** module (migrating B if needed). `pinB` is given the inverted polarity so it's the phase-complement of `pinA`. `deadTimeTicks` shaves on-time off `pinB` to prevent shoot-through. |

The mental model: `analogWrite` keeps the old "one shared timer" feel by default. To get a true independent group, set the frequency first.

## API

```cpp
// Capability queries (Arduino.h)
uint8_t  nrfPwmChannelCapacity();           // 16
uint8_t  nrfPwmActiveChannels();
uint8_t  nrfPwmTimerGroupCount();           // 4
bool     nrfPwmIndependentTimersSupported();// true
bool     nrfPwmSharedTimer();               // false (not all-shared any more)
bool     nrfPwmPolarityConfigurable();      // true
bool     nrfPwmCanAllocateChannel(uint8_t pin);

// Per-pin frequency / module assignment
bool     nrfPwmSetPinFrequency(uint8_t pin, uint32_t hz);
uint32_t nrfPwmPinFrequencyHz(uint8_t pin);
uint8_t  nrfPwmPinTimerGroup(uint8_t pin);  // 0..3, or 0xFF if not bound

// Per-pin polarity (default HIGH_ON_DUTY matches stock analogWrite)
#define NRF_PWM_PIN_POLARITY_HIGH_ON_DUTY 0U
#define NRF_PWM_PIN_POLARITY_LOW_ON_DUTY  1U
bool     nrfPwmSetPinPolarity(uint8_t pin, uint8_t polarity);
uint8_t  nrfPwmPinPolarity(uint8_t pin);

// Half-bridge pair with software dead-time
bool     nrfPwmConfigureComplementary(uint8_t pinA, uint8_t pinB, uint16_t deadTimeTicks);

// Legacy globals (still work; now treated as the system default)
bool     analogWriteFrequency(uint32_t hz); // sets every module's freq
uint32_t analogWriteFrequencyHz();
void     analogWriteResolution(int bits);
```

## Build-time defaults — Tools → PWM speed

`boards.txt` adds a `menu.pwmclock` for the verified ProMicro:

| Menu | Default carrier | What it sets |
|---|---|---|
| **Auto** (default) | ~977 Hz | prescaler 4 / top 1023 — Arduino-classic feel |
| **High-speed** | ~16 kHz | prescaler 0 / top 999 — power MOSFETs, switching audio |
| **Low-speed** | ~125 Hz | prescaler 7 / top 999 — LED breathing, servo-period range |

These only bias the silent default carrier used by `analogWrite(pin, v)` without an explicit frequency call. `nrfPwmSetPinFrequency()` always overrides per-group. The build flags are `NRF_PWM_DEFAULT_PRESCALER` and `NRF_PWM_DEFAULT_COUNTERTOP`.

## Example

`examples/PWMMultiModule/PWMMultiModule.ino` shows four pins at four frequencies, a polarity override, and a complementary pair with dead-time:

```cpp
const uint8_t pins[4]   = { LED_BUILTIN, LED_RED, LED_GREEN, LED_BLUE };
const uint32_t freqs[4] = {   500UL,      2000UL,  10000UL,   50000UL };
for (uint8_t i = 0; i < 4; ++i) {
  nrfPwmSetPinFrequency(pins[i], freqs[i]); // each pin -> its own group
  analogWrite(pins[i], 128);
}
nrfPwmSetPinPolarity(LED_RED, NRF_PWM_PIN_POLARITY_LOW_ON_DUTY);
nrfPwmConfigureComplementary(LED_GREEN, LED_BLUE, /*deadTimeTicks=*/4);
```

## Verification

Compiles clean for all three `pwmclock` menu options on `promicro_nrf52840`. SWD verification on the running sketch confirmed multi-module allocation (slots `0x00` / `0x10` simultaneously for two pins on PWM0 and PWM1).

## Known caveats

- All four PWM modules share the nRF52's HFCLK source, so the available counter clocks are `16 MHz / 1, /2, /4, /8, /16, /32, /64, /128` (prescaler 0..7) — no LFCLK option. "Low-speed" means a large prescaler, not a different clock.
- Channel polarity is per-channel in hardware via the SEQ bit-15 flag, but it interacts with the counter direction. The library always uses the up-counting mode; `LOW_ON_DUTY` is implemented by clearing bit 15 of the SEQ value so the channel's HIGH window becomes the complement of the requested duty.
- Dead-time is software-emulated by shaving `deadTimeTicks/2` from the on-time on `pinB`; the hardware has no native dead-time generator. The math is approximate at very low duty cycles.
