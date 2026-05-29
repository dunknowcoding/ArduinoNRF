// NimBLE.h - Arduino API surface for the planned NimBLE (Apache Mynewt)
// integration on the nRF52840 RADIO. See
// docs/platform/NIMBLE_INTEGRATION_PLAN.md for the milestone schedule.
//
// SCOPE
//   This is a SKELETON. The actual BLE host + controller requires the
//   NimBLE source tree vendored under vendor/ (see vendor/README.md).
//   Until then every operation returns NIMBLE_NOT_VENDORED and
//   isAvailable() returns false. The bring-up API is intentionally
//   minimal here so user code can be written against it now; the full
//   GATT server / client surface lands with milestones M2..M5.

#pragma once

#include <stdint.h>
#include <stddef.h>

class NimBLE {
public:
    enum Status : int8_t {
        NIMBLE_OK            =  0,
        NIMBLE_NOT_VENDORED  = -1,   // NimBLE source not vendored
        NIMBLE_NOT_STARTED   = -2,
        NIMBLE_BAD_PARAM     = -3,
        NIMBLE_INTERNAL      = -4,
    };

    // -- Bring-up ----------------------------------------------------------

    // Initialize the controller + host. `deviceName` is the local name
    // advertised in scan responses (max 30 chars). Returns NIMBLE_OK on
    // real hardware after a successful BLE stack init.
    static Status begin(const char *deviceName);

    static void end();
    static bool isAvailable();

    // -- Advertising (M2 milestone) ----------------------------------------

    // Default advertising payload: device name + a placeholder service UUID.
    // The full GAP / GATT service-builder API lands with M3.
    static Status startAdvertising();
    static Status stopAdvertising();

    // -- Connection state (M3 milestone) -----------------------------------

    static bool isConnected();
    static int  connectionCount();

    // -- Diagnostics -------------------------------------------------------

    // Public BLE address (6 bytes). On nRF52840 this is derived from
    // FICR.DEVICEADDR (factory-unique). Filled with 0xFF if NimBLE isn't
    // initialized.
    static void getPublicAddress(uint8_t addr[6]);
};
