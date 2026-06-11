# PWM Behavior

Status: current package truth. (This page once documented the original
single-module facade; the core has since grown to drive all four PWM
peripherals — full details in
[PWM_MULTI_MODULE.md](PWM_MULTI_MODULE.md).)

## Current implementation boundary

- The core drives **all four** nRF52840 PWM modules (PWM0–PWM3): up to
  **16 simultaneous channels** in **4 independent frequency groups**.
- Capability surface: `nrfPwmTimerGroupCount()` returns `4`,
  `nrfPwmIndependentTimersSupported()` returns `true`,
  `nrfPwmChannelCapacity()` returns `16`.
- Duty values are independent per channel; **frequency is shared per module**
  (a hardware property — each module is one frequency group).
- Plain `analogWrite(pin, value)` consolidates pins onto already-active
  modules, preserving the classic "one shared timer" feel; call
  `nrfPwmSetPinFrequency(pin, hz)` first to give a pin its own group.

## What the hardware could do versus what this core exposes

The four hardware PWM peripherals are all exposed. What remains hardware-
bound: 4 channels per module sharing that module's frequency, HFCLK-derived
counter clocks only (`16 MHz / 2^prescaler`, prescaler 0..7), and no native
dead-time generator (the complementary-pair API emulates dead-time in
software). See [PWM_MULTI_MODULE.md](PWM_MULTI_MODULE.md) for the
allocation model, API list, and Tools → PWM speed menu.
