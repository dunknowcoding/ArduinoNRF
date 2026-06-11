# PWM Edge and Timer Boundaries

(Updated for the multi-module facade — see
[PWM_MULTI_MODULE.md](PWM_MULTI_MODULE.md) for the full model.)

- The core exposes **all four** PWM modules: up to 16 routed pins at once,
  in 4 independent frequency groups (one per module).
- Duty cycles are independent per channel; frequency is shared within a
  module's group.
- `analogWrite()` alone keeps pins consolidated on one module (legacy feel);
  `nrfPwmSetPinFrequency(pin, hz)` claims an idle module for a new frequency
  group or joins an existing group at the same frequency.

## Frequency truth

- Counter clocks are HFCLK-derived only: `16 MHz / 2^prescaler`,
  prescaler 0..7. There is no LFCLK option — "low speed" means a large
  prescaler.
- A requested frequency is realized as `clock / (COUNTERTOP + 1)`; the core
  picks the prescaler + counter-top pair for the target and
  `nrfPwmPinFrequencyHz(pin)` reports what was actually achieved.
- The default carrier (no explicit frequency call) comes from the
  Tools → PWM speed menu: ~977 Hz (Auto, top 1023 → native 10-bit duty),
  ~16 kHz (High-speed), or ~125 Hz (Low-speed).
- Native duty resolution is the counter-top: 10 bits at the Auto default;
  `analogWriteResolution()` rescales sketch-side values onto it.
