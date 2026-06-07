// Thread.h - Arduino API surface for the planned OpenThread integration.
// See vendor/README.md.
//
// SKELETON - returns THREAD_NOT_VENDORED until vendoring is done.
#pragma once

#include <stdint.h>
#include <stddef.h>

class Thread {
public:
    enum Status : int8_t {
        THREAD_OK            =  0,
        THREAD_NOT_VENDORED  = -1,
        THREAD_NOT_STARTED   = -2,
        THREAD_BAD_PARAM     = -3,
        THREAD_INTERNAL      = -4,
    };

    // Thread device roles. MTD = Minimal Thread Device (always-on FFD-like
    // behavior, no routing). MED = Minimal End Device (can sleep between
    // polls). SED = Sleepy End Device (deep sleep, longest latency).
    enum Role : uint8_t {
        ROLE_MTD = 1,
        ROLE_MED = 2,
        ROLE_SED = 3,
    };

    // Initialize the OpenThread stack and PHY.
    static Status begin(Role role);
    static void   end();
    static bool   isAvailable();

    // -- Network commissioning (M2 milestone) ------------------------------

    // Join an existing Thread network. networkName / extendedPanId / masterKey
    // are the standard commissioning parameters; on success the device
    // attaches to the network.
    static Status joinNetwork(const char *networkName,
                               const uint8_t extendedPanId[8],
                               const uint8_t masterKey[16],
                               uint8_t channel,
                               uint16_t panId);

    // Are we currently attached to a Thread network?
    static bool isAttached();

    // -- CoAP server (M3 milestone) ----------------------------------------

    typedef void (*coapHandler_t)(const char *payload, size_t length);

    // Register a handler for incoming CoAP GET requests on `path`.
    static Status onCoapGet(const char *path, coapHandler_t handler);

    // -- Identity ----------------------------------------------------------

    // EUI-64 (factory-unique, derived from FICR.DEVICEADDR).
    static void getEui64(uint8_t eui[8]);
};
