// platform_impl.cpp - Thread M2 implementation of the OpenThread platform
// abstraction (otPlat*) for nRF52840 bare-metal Arduino.
//
// What's in M2 (this file):
//   * Real implementations for the platform functions that need ZERO
//     external vendoring - they sit on top of the core's existing
//     drivers (NrfRtc / NrfRng / NrfPower / FICR registers).
//   * Stubs for the platform contract that the OpenThread core will pull
//     in at M3+: they return OT_ERROR_NOT_IMPLEMENTED so any uncovered
//     code path is loud about being a stub.
//
// What lands in M3:
//   * Radio driver (otPlatRadio*) - the most complex single block,
//     needs the nrf-802154 PHY driver vendored from nrfxlib alongside
//     the OpenThread core itself.
//   * Crypto (otPlatCrypto*) - hooked into mbedTLS or CC310 once that
//     library lands.
//   * DNS-SD / mDNS / SRP / TREL - infrastructure services; only Border
//     Router roles need them.
//   * Flash settings (otPlatFlash*, otPlatSettings*) - wires to NrfNvmc.
//   * Logging output - currently a no-op; M3 routes to Serial.
//
// IMPORTANT: At M2 the OpenThread core source itself is NOT yet vendored,
// so this file's only consumer is Thread.cpp's smoke test. The functions
// below all link cleanly when OpenThread core arrives.
//
// File is C++ so it can directly use the core's NrfRtc / NrfTimer / NrfRng
// classes, but every otPlat* symbol is wrapped in extern "C" so OpenThread's
// C callers resolve them by their plain C ABI names.

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "../../../cores/arduino/NrfRtc.h"
#include "../../../cores/arduino/NrfPeripherals.h"

extern "C" {

#include "openthread/error.h"
#include "openthread/instance.h"

// =============================================================================
// Real implementations
// =============================================================================

// ---- Alarm (millisecond) ----------------------------------------------------
//
// OpenThread expects a free-running millisecond counter and the ability to
// schedule a wakeup at an absolute tick. Backed by RTC2 (LFCLK / 32 -> 1 kHz
// tick). The core's NrfRtc already exposes RTC2 via a singleton; we drive its
// CC0 channel here.

static volatile bool s_milli_alarm_active = false;

static void onMilliAlarm(void) {
    s_milli_alarm_active = false;
    // M3: call otPlatAlarmMilliFired(aInstance) here, once the OT core
    // is in place. For now the firing event just clears state.
}

void otPlatAlarmMilliInit(void) {
    NrfRtc &rtc = nrfRtc2();
    if (!rtc.isRunning()) {
        rtc.begin(1000U);
        rtc.attachCompareInterrupt(0, onMilliAlarm);
        rtc.start();
    }
}

uint32_t otPlatAlarmMilliGetNow(void) {
    // RTC2 ticks at 1 kHz. The 24-bit counter wraps every ~16777 seconds
    // (~4.7 hours); the OT alarm API uses 32-bit ms with wrap-tolerant
    // comparison.
    NrfRtc &rtc = nrfRtc2();
    return rtc.isRunning() ? rtc.counter() : 0U;
}

void otPlatAlarmMilliStartAt(otInstance *aInstance, uint32_t aT0, uint32_t aDt) {
    (void)aInstance;
    NrfRtc &rtc = nrfRtc2();
    if (!rtc.isRunning()) {
        otPlatAlarmMilliInit();
    }
    rtc.setCompare(0, (aT0 + aDt) & 0x00FFFFFFUL);
    s_milli_alarm_active = true;
}

void otPlatAlarmMilliStop(otInstance *aInstance) {
    (void)aInstance;
    s_milli_alarm_active = false;
    nrfRtc2().detachCompareInterrupt(0);
}

// ---- Alarm (microsecond) ----------------------------------------------------
//
// Backed by TIMER3 (free for non-NimBLE/Zigbee builds), 1 MHz tick.

void otPlatAlarmMicroInit(void) {
    NrfTimer &t = nrfTimer3();
    if (!t.isRunning()) {
        t.begin(1000000U);
        t.start();
    }
}

uint32_t otPlatAlarmMicroGetNow(void) {
    NrfTimer &t = nrfTimer3();
    return t.isRunning() ? t.counter() : 0U;
}

void otPlatAlarmMicroStartAt(otInstance *aInstance, uint32_t aT0, uint32_t aDt) {
    (void)aInstance;
    NrfTimer &t = nrfTimer3();
    if (!t.isRunning()) {
        otPlatAlarmMicroInit();
    }
    t.setCompare(0, aT0 + aDt);
}

void otPlatAlarmMicroStop(otInstance *aInstance) {
    (void)aInstance;
    nrfTimer3().detachCompareInterrupt(0);
}

// ---- Entropy / RNG ---------------------------------------------------------

otError otPlatEntropyGet(uint8_t *aOutput, uint16_t aOutputLength) {
    if (aOutput == NULL || aOutputLength == 0) {
        return OT_ERROR_INVALID_ARGS;
    }
    NrfRng::randomBytes(aOutput, aOutputLength);
    return OT_ERROR_NONE;
}

// ---- Memory ---------------------------------------------------------------

void *otPlatCAlloc(size_t aNum, size_t aSize) {
    return calloc(aNum, aSize);
}

void otPlatFree(void *aPtr) {
    free(aPtr);
}

// ---- Misc -----------------------------------------------------------------

void otPlatReset(otInstance *aInstance) {
    (void)aInstance;
    // NVIC SystemReset.
    *(volatile uint32_t *)0xE000ED0C = 0x05FA0004UL;
    while (1) {}
}

otError otPlatResetToBootloader(otInstance *aInstance) {
    (void)aInstance;
    // GPREGRET magic 0x57 then SYSRESETREQ - same as the verified hands-free
    // upload path the rest of the core uses.
    *(volatile uint32_t *)0x4000051C = 0x57UL;
    *(volatile uint32_t *)0xE000ED0C = 0x05FA0004UL;
    while (1) {}
    return OT_ERROR_NONE;   // unreachable
}

int otPlatGetResetReason(otInstance *aInstance) {
    (void)aInstance;
    return 0;   // OT_PLAT_RESET_REASON_POWER_ON - M3 maps from POWER_RESETREAS
}

void otPlatAssertFail(const char *aFilename, int aLineNumber) {
    (void)aFilename;
    (void)aLineNumber;
    // Trip a system reset so the existing debug path catches it.
    *(volatile uint32_t *)0xE000ED0C = 0x05FA0004UL;
    while (1) {}
}

void otPlatWakeHost(void) {
    // No-op on this single-chip platform.
}

// ---- Radio (EUI-64 only at M2; full radio in M3) --------------------------

void otPlatRadioGetIeeeEui64(otInstance *aInstance, uint8_t *aIeeeEui64) {
    (void)aInstance;
    if (aIeeeEui64 == NULL) return;
    // FICR.DEVICEID0/DEVICEID1 - factory-unique 64-bit ID.
    const uint32_t devid0 = *(volatile uint32_t *)0x10000060UL;
    const uint32_t devid1 = *(volatile uint32_t *)0x10000064UL;
    aIeeeEui64[0] = (uint8_t)(devid0 >> 0);
    aIeeeEui64[1] = (uint8_t)(devid0 >> 8);
    aIeeeEui64[2] = (uint8_t)(devid0 >> 16);
    aIeeeEui64[3] = (uint8_t)(devid0 >> 24);
    aIeeeEui64[4] = (uint8_t)(devid1 >> 0);
    aIeeeEui64[5] = (uint8_t)(devid1 >> 8);
    aIeeeEui64[6] = (uint8_t)(devid1 >> 16);
    aIeeeEui64[7] = (uint8_t)(devid1 >> 24);
}

// =============================================================================
// Stub implementations - return NOT_IMPLEMENTED so any M3 caller knows the
// function exists but is awaiting a real backend.
// =============================================================================

otError otPlatFlashErase(otInstance *aInstance, uint8_t aSwapIndex) {
    (void)aInstance; (void)aSwapIndex;
    return OT_ERROR_NOT_IMPLEMENTED;
}

void otPlatLog(int aLogLevel, int aLogRegion, const char *aFormat, ...) {
    (void)aLogLevel; (void)aLogRegion; (void)aFormat;
    // M3: route to Serial.
}

void otPlatSettingsInit(otInstance *aInstance, const uint16_t *aSensitiveKeys, uint16_t aSensitiveKeysLength) {
    (void)aInstance; (void)aSensitiveKeys; (void)aSensitiveKeysLength;
}

otError otPlatSettingsGet(otInstance *aInstance, uint16_t aKey, int aIndex, uint8_t *aValue, uint16_t *aValueLength) {
    (void)aInstance; (void)aKey; (void)aIndex; (void)aValue; (void)aValueLength;
    return OT_ERROR_NOT_FOUND;
}

otError otPlatSettingsSet(otInstance *aInstance, uint16_t aKey, const uint8_t *aValue, uint16_t aValueLength) {
    (void)aInstance; (void)aKey; (void)aValue; (void)aValueLength;
    return OT_ERROR_NOT_IMPLEMENTED;
}

otError otPlatSettingsAdd(otInstance *aInstance, uint16_t aKey, const uint8_t *aValue, uint16_t aValueLength) {
    (void)aInstance; (void)aKey; (void)aValue; (void)aValueLength;
    return OT_ERROR_NOT_IMPLEMENTED;
}

otError otPlatSettingsDelete(otInstance *aInstance, uint16_t aKey, int aIndex) {
    (void)aInstance; (void)aKey; (void)aIndex;
    return OT_ERROR_NOT_IMPLEMENTED;
}

void otPlatSettingsWipe(otInstance *aInstance) { (void)aInstance; }
void otPlatSettingsDeinit(otInstance *aInstance) { (void)aInstance; }

}   // extern "C"
