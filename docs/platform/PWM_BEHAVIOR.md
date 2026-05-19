# PWM Behavior

Date: 2026-05-04

Status: current package truth, not a future design promise.

## Current implementation boundary

- The current Arduino core exposes one shared PWM timer group.
- The routed channel capacity is `4` active pins at once.
- Duty values are independent per allocated channel.
- Frequency is shared across the whole active group.
- `nrfPwmTimerGroupCount()` returns `1` and `nrfPwmIndependentTimersSupported()` returns `false`.

## What the hardware could do versus what this core exposes

- nRF52840 hardware has four PWM peripherals.
- The current core exposes only the first group through the Arduino API surface.
- That means the package models a practical four-channel routed limit today, not the theoretical aggregate capacity of all hardware PWM peripherals.
