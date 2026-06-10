// Thread.h - Arduino API for OpenThread on the nRF52840's native RADIO.
//
// The OpenThread core (vendored, see ARDUINONRF-PATCH markers) runs on a
// register-level 802.15.4 driver in ot_radio_nrf52840.cpp and the platform
// backends in platform_impl.cpp. Everything is polled from process();
// sketches MUST call Thread.process() from loop() (and while waiting).
//
// Typical leader/router node:
//
//   const uint8_t key[16] = {...};
//   Thread.begin();
//   Thread.setNetwork("ArduinoNRF", 11, 0xBEEF, key);
//   Thread.start();
//   void loop() { Thread.process(); }
//
// Peripheral footprint: RADIO (exclusive with NimBLE/Zigbee/NrfRadio),
// RTC2 (ms alarm), TIMER3 (us alarm + timestamps).
#pragma once

#include <stdint.h>
#include <stddef.h>

struct otInstance;

class ThreadClass {
public:
    enum Status : int8_t {
        THREAD_OK          =  0,
        THREAD_NOT_STARTED = -2,
        THREAD_BAD_PARAM   = -3,
        THREAD_INTERNAL    = -4,
    };

    // Mirrors otDeviceRole.
    enum Role : uint8_t {
        ROLE_DISABLED = 0,
        ROLE_DETACHED = 1,
        ROLE_CHILD    = 2,
        ROLE_ROUTER   = 3,
        ROLE_LEADER   = 4,
    };

    // Bring up the platform (alarms, radio, crypto heap) and create the
    // OpenThread instance. Must be called before anything else.
    static Status begin();
    static void   end();
    static bool   isAvailable();

    // Install the operational dataset. Both nodes of a network must use the
    // same name / channel (11..26) / PAN id / 16-byte network key.
    // extendedPanId is optional (8 bytes); defaults to a fixed value.
    static Status setNetwork(const char    *networkName,
                             uint8_t        channel,
                             uint16_t       panId,
                             const uint8_t  networkKey[16],
                             const uint8_t  extendedPanId[8] = nullptr);

    // Interface up + Thread protocol start (attach or form a partition).
    static Status start();
    static Status stop();

    // Event pump: alarms, radio events, OpenThread tasklets. Call from
    // loop() as often as possible.
    static void process();

    static Role        role();
    static const char *roleString();
    static bool        isAttached();   // child, router or leader

    // RLOC16 and leader info are handy for bring-up diagnostics.
    static uint16_t rloc16();

    // EUI-64 (factory-unique, derived from FICR.DEVICEID).
    static void getEui64(uint8_t eui[8]);

    // Escape hatch: the raw otInstance for direct OpenThread API use.
    static otInstance *instance();
};

extern ThreadClass Thread;
