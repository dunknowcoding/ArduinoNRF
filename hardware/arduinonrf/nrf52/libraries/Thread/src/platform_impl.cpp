// platform_impl.cpp - OpenThread platform abstraction (otPlat*) for the
// ArduinoNRF nRF52840 bare-metal core. Radio lives in ot_radio_nrf52840.cpp.
//
// Time bases:
//   * Millisecond alarm - RTC2 at 32768 Hz (prescaler 0, exact LFCLK ticks),
//     extended to 64 bits with the overflow interrupt; ms = ticks * 125 / 4096.
//   * Microsecond alarm - TIMER3 at 1 MHz, 32-bit free-running.
//
// Both alarms are POLLED: ISRs never call into the OT core (it is not
// ISR-safe). nrfOtPlatformProcess() compares "now" against the armed targets
// with wrap-tolerant math and fires otPlatAlarm*Fired() from the main loop.
// The Thread example sketches call Thread.process() every loop() pass, so
// alarm latency is one loop iteration.
//
// Settings are a RAM key-value store: functional for bring-up (a reboot
// loses the dataset and the device re-attaches), to be replaced by an NVMC
// flash backend.

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Arduino.h>

#include "../../../cores/arduino/NrfRtc.h"
#include "../../../cores/arduino/NrfPeripherals.h"

#include "openthread-core-config.h"

extern "C" {

#include <openthread/error.h>
#include <openthread/instance.h>
#include <openthread/tasklet.h>
#include <openthread/platform/alarm-milli.h>
#include <openthread/platform/alarm-micro.h>
#include <openthread/platform/entropy.h>
#include <openthread/platform/logging.h>
#include <openthread/platform/memory.h>
#include <openthread/platform/misc.h>
#include <openthread/platform/radio.h>
#include <openthread/platform/settings.h>

}

#include "ot_platform_arduino.h"

#if !OPENTHREAD_CONFIG_HEAP_EXTERNAL_ENABLE
#include "mbedtls/version.h"
#if defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C)
#include "mbedtls/memory_buffer_alloc.h"
#endif
#endif

// =============================================================================
// Millisecond time base (RTC2)
// =============================================================================

static volatile uint32_t sRtcOverflows; // each overflow = 2^24 LFCLK ticks

static void onRtcOverflow(void)
{
    sRtcOverflows++;
}

static uint64_t rtcTicks64(void)
{
    uint32_t ovf1, ovf2, cnt;

    do
    {
        ovf1 = sRtcOverflows;
        cnt  = nrfRtc2().counter();
        ovf2 = sRtcOverflows;
    } while (ovf1 != ovf2);

    return ((uint64_t)ovf2 << 24) | cnt;
}

static uint64_t nowMs64(void)
{
    // 32768 Hz -> ms: * 1000 / 32768 == * 125 / 4096
    return (rtcTicks64() * 125U) >> 12;
}

static bool     sMilliArmed;
static uint32_t sMilliTarget;

extern "C" void otPlatAlarmMilliInit(void)
{
    NrfRtc &rtc = nrfRtc2();
    if (!rtc.isRunning())
    {
        rtc.begin(32768U); // prescaler 0: exact LFCLK ticks
        rtc.attachOverflowInterrupt(onRtcOverflow);
        rtc.start();
    }
}

extern "C" uint32_t otPlatAlarmMilliGetNow(void)
{
    return (uint32_t)nowMs64();
}

extern "C" void otPlatAlarmMilliStartAt(otInstance *aInstance, uint32_t aT0, uint32_t aDt)
{
    (void)aInstance;
    sMilliTarget = aT0 + aDt;
    sMilliArmed  = true;
}

extern "C" void otPlatAlarmMilliStop(otInstance *aInstance)
{
    (void)aInstance;
    sMilliArmed = false;
}

// =============================================================================
// Microsecond time base (TIMER3)
// =============================================================================

static bool     sMicroArmed;
static uint32_t sMicroTarget;

extern "C" void otPlatAlarmMicroInit(void)
{
    NrfTimer &t = nrfTimer3();
    if (!t.isRunning())
    {
        t.begin(1000000U);
        t.start();
    }
}

extern "C" uint32_t otPlatAlarmMicroGetNow(void)
{
    return nrfTimer3().counter();
}

extern "C" void otPlatAlarmMicroStartAt(otInstance *aInstance, uint32_t aT0, uint32_t aDt)
{
    (void)aInstance;
    sMicroTarget = aT0 + aDt;
    sMicroArmed  = true;
}

extern "C" void otPlatAlarmMicroStop(otInstance *aInstance)
{
    (void)aInstance;
    sMicroArmed = false;
}

// =============================================================================
// Entropy (TRNG)
// =============================================================================

extern "C" otError otPlatEntropyGet(uint8_t *aOutput, uint16_t aOutputLength)
{
    if (aOutput == NULL || aOutputLength == 0)
    {
        return OT_ERROR_INVALID_ARGS;
    }
    NrfRng::randomBytes(aOutput, aOutputLength);
    return OT_ERROR_NONE;
}

// =============================================================================
// Memory
// =============================================================================

extern "C" void *otPlatCAlloc(size_t aNum, size_t aSize) { return calloc(aNum, aSize); }
extern "C" void  otPlatFree(void *aPtr) { free(aPtr); }

// =============================================================================
// Misc
// =============================================================================

extern "C" void otPlatReset(otInstance *aInstance)
{
    (void)aInstance;
    *(volatile uint32_t *)0xE000ED0C = 0x05FA0004UL; // AIRCR.SYSRESETREQ
    while (1)
    {
    }
}

extern "C" otError otPlatResetToBootloader(otInstance *aInstance)
{
    (void)aInstance;
    // GPREGRET magic 0x57 + SYSRESETREQ - the verified hands-free DFU path.
    *(volatile uint32_t *)0x4000051C = 0x57UL;
    *(volatile uint32_t *)0xE000ED0C = 0x05FA0004UL;
    while (1)
    {
    }
    return OT_ERROR_NONE; // unreachable
}

extern "C" otPlatResetReason otPlatGetResetReason(otInstance *aInstance)
{
    (void)aInstance;
    return OT_PLAT_RESET_REASON_POWER_ON;
}

extern "C" void otPlatAssertFail(const char *aFilename, int aLineNumber)
{
    if (Serial)
    {
        Serial.print("OT ASSERT ");
        Serial.print(aFilename);
        Serial.print(":");
        Serial.println(aLineNumber);
        Serial.flush();
    }
    *(volatile uint32_t *)0xE000ED0C = 0x05FA0004UL;
    while (1)
    {
    }
}

extern "C" void otPlatWakeHost(void)
{
    // Single-chip platform: no host to wake.
}

// =============================================================================
// Radio identity (the radio driver itself is in ot_radio_nrf52840.cpp)
// =============================================================================

extern "C" void otPlatRadioGetIeeeEui64(otInstance *aInstance, uint8_t *aIeeeEui64)
{
    (void)aInstance;
    if (aIeeeEui64 == NULL)
    {
        return;
    }
    const uint32_t devid0 = *(volatile uint32_t *)0x10000060UL; // FICR.DEVICEID[0]
    const uint32_t devid1 = *(volatile uint32_t *)0x10000064UL; // FICR.DEVICEID[1]
    aIeeeEui64[0] = (uint8_t)(devid1 >> 24) | 0x02U; // locally administered
    aIeeeEui64[1] = (uint8_t)(devid1 >> 16);
    aIeeeEui64[2] = (uint8_t)(devid1 >> 8);
    aIeeeEui64[3] = (uint8_t)(devid1 >> 0);
    aIeeeEui64[4] = (uint8_t)(devid0 >> 24);
    aIeeeEui64[5] = (uint8_t)(devid0 >> 16);
    aIeeeEui64[6] = (uint8_t)(devid0 >> 8);
    aIeeeEui64[7] = (uint8_t)(devid0 >> 0);
}

// =============================================================================
// Logging -> Serial
// =============================================================================

extern "C" void otPlatLog(otLogLevel aLogLevel, otLogRegion aLogRegion, const char *aFormat, ...)
{
    (void)aLogRegion;

    if (!Serial)
    {
        return;
    }

    char    buf[160];
    va_list args;

    va_start(args, aFormat);
    vsnprintf(buf, sizeof(buf), aFormat, args);
    va_end(args);

    Serial.print("[OT");
    Serial.print((int)aLogLevel);
    Serial.print("] ");
    Serial.println(buf);
}

// =============================================================================
// Settings - RAM-backed key-value store (volatile; NVMC backend is future work)
// =============================================================================

#define SETTINGS_SLOT_COUNT 24
#define SETTINGS_VALUE_MAX  144

struct SettingsSlot
{
    bool     used;
    uint16_t key;
    uint16_t length;
    uint8_t  value[SETTINGS_VALUE_MAX];
};

static SettingsSlot sSettings[SETTINGS_SLOT_COUNT];

extern "C" void otPlatSettingsInit(otInstance *aInstance, const uint16_t *aSensitiveKeys, uint16_t aSensitiveKeysLength)
{
    (void)aInstance;
    (void)aSensitiveKeys;
    (void)aSensitiveKeysLength;
}

extern "C" void otPlatSettingsDeinit(otInstance *aInstance) { (void)aInstance; }

extern "C" otError otPlatSettingsGet(otInstance *aInstance, uint16_t aKey, int aIndex, uint8_t *aValue, uint16_t *aValueLength)
{
    (void)aInstance;

    int match = 0;

    for (int i = 0; i < SETTINGS_SLOT_COUNT; i++)
    {
        if (!sSettings[i].used || sSettings[i].key != aKey)
        {
            continue;
        }
        if (match == aIndex)
        {
            if (aValueLength != NULL)
            {
                uint16_t copyLen = sSettings[i].length;

                if (aValue != NULL)
                {
                    if (copyLen > *aValueLength)
                    {
                        copyLen = *aValueLength;
                    }
                    memcpy(aValue, sSettings[i].value, copyLen);
                }
                *aValueLength = sSettings[i].length;
            }
            return OT_ERROR_NONE;
        }
        match++;
    }

    return OT_ERROR_NOT_FOUND;
}

extern "C" otError otPlatSettingsAdd(otInstance *aInstance, uint16_t aKey, const uint8_t *aValue, uint16_t aValueLength)
{
    (void)aInstance;

    if (aValueLength > SETTINGS_VALUE_MAX)
    {
        return OT_ERROR_NO_BUFS;
    }

    for (int i = 0; i < SETTINGS_SLOT_COUNT; i++)
    {
        if (!sSettings[i].used)
        {
            sSettings[i].used   = true;
            sSettings[i].key    = aKey;
            sSettings[i].length = aValueLength;
            if (aValueLength > 0 && aValue != NULL)
            {
                memcpy(sSettings[i].value, aValue, aValueLength);
            }
            return OT_ERROR_NONE;
        }
    }

    return OT_ERROR_NO_BUFS;
}

extern "C" otError otPlatSettingsDelete(otInstance *aInstance, uint16_t aKey, int aIndex)
{
    (void)aInstance;

    otError err   = OT_ERROR_NOT_FOUND;
    int     match = 0;

    for (int i = 0; i < SETTINGS_SLOT_COUNT; i++)
    {
        if (!sSettings[i].used || sSettings[i].key != aKey)
        {
            continue;
        }
        if (aIndex < 0 || match == aIndex)
        {
            sSettings[i].used = false;
            err = OT_ERROR_NONE;
            if (aIndex >= 0)
            {
                break;
            }
        }
        match++;
    }

    return err;
}

extern "C" otError otPlatSettingsSet(otInstance *aInstance, uint16_t aKey, const uint8_t *aValue, uint16_t aValueLength)
{
    (void)otPlatSettingsDelete(aInstance, aKey, -1);
    return otPlatSettingsAdd(aInstance, aKey, aValue, aValueLength);
}

extern "C" void otPlatSettingsWipe(otInstance *aInstance)
{
    (void)aInstance;
    memset(sSettings, 0, sizeof(sSettings));
}

// =============================================================================
// Tasklets - polled from Thread.process(), so the signal is a no-op
// =============================================================================

extern "C" void otTaskletsSignalPending(otInstance *aInstance)
{
    (void)aInstance;
}

// =============================================================================
// Init + event pump
// =============================================================================

extern "C" void nrfOtPlatformInit(void)
{
    otPlatAlarmMilliInit();
    otPlatAlarmMicroInit();

#if !OPENTHREAD_CONFIG_HEAP_EXTERNAL_ENABLE && defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C)
    // mbedtls runs on its static-buffer allocator (no libc heap): md/hmac
    // contexts allocate from here during MLE key derivation.
    static uint8_t sMbedtlsHeap[8192];
    mbedtls_memory_buffer_alloc_init(sMbedtlsHeap, sizeof(sMbedtlsHeap));
#endif
}

extern "C" void nrfOtPlatformProcess(otInstance *aInstance)
{
    if (sMilliArmed)
    {
        uint32_t now = otPlatAlarmMilliGetNow();

        if ((int32_t)(now - sMilliTarget) >= 0)
        {
            sMilliArmed = false;
            otPlatAlarmMilliFired(aInstance);
        }
    }

#if OPENTHREAD_CONFIG_PLATFORM_USEC_TIMER_ENABLE
    if (sMicroArmed)
    {
        uint32_t now = otPlatAlarmMicroGetNow();

        if ((int32_t)(now - sMicroTarget) >= 0)
        {
            sMicroArmed = false;
            otPlatAlarmMicroFired(aInstance);
        }
    }
#endif
}
