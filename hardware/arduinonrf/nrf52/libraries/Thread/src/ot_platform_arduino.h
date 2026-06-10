// ot_platform_arduino.h - internal glue between the Arduino-facing Thread
// class and the otPlat* platform backends in this library. Not a public API.
#pragma once

#include <stdint.h>

struct otInstance;

#ifdef __cplusplus
extern "C" {
#endif

// platform_impl.cpp - alarms (RTC2 ms / TIMER3 us), entropy, RAM settings,
// log sink, mbedtls heap. Init once before otInstanceInitSingle().
void nrfOtPlatformInit(void);
// Drain pending alarm events; call from the main loop, never from an ISR.
void nrfOtPlatformProcess(otInstance *aInstance);

// ot_radio_nrf52840.cpp - IEEE 802.15.4 RADIO driver.
void nrfOtRadioInit(void);
// Deliver queued receive/transmit-done events into the OpenThread core.
void nrfOtRadioProcess(otInstance *aInstance);

#ifdef __cplusplus
}
#endif
