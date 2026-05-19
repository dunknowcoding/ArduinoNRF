# PWM Edge and Timer Boundaries

- This core currently exposes a single PWM timer group through `PWM0`.
- Up to four routed pins can be active at once.
- Duty cycles are independent per active channel, but frequency is shared across the group.
- `PWM1`, `PWM2`, and `PWM3` are not exposed by the current Arduino API surface.

## Frequency truth

- `COUNTERTOP` stays fixed at `1023`, so native PWM resolution is 10 bits.
- `nrfPwmSetFrequency()` only accepts exact prescaler-derived values.
- The currently supported frequencies are `15625`, `7812`, `3906`, `1953`, `976`, `488`, `244`, and `122` Hz.
