// Zigbee.h - Arduino API surface for the planned Zboss + nrf-802154
// integration. See vendor/README.md.
//
// SKELETON - returns ZIGBEE_NOT_VENDORED until vendoring is done.
#pragma once

#include <stdint.h>
#include <stddef.h>

class Zigbee {
public:
    enum Status : int8_t {
        ZIGBEE_OK            =  0,
        ZIGBEE_NOT_VENDORED  = -1,
        ZIGBEE_NOT_STARTED   = -2,
        ZIGBEE_BAD_PARAM     = -3,
        ZIGBEE_INTERNAL      = -4,
    };

    enum Role : uint8_t {
        ROLE_COORDINATOR = 1,
        ROLE_ROUTER      = 2,
        ROLE_END_DEVICE  = 3,
    };

    // Bring up the 802.15.4 PHY + Zboss stack with the given role and PAN ID.
    // The Trust Center Link Key (TCLK) is the well-known default
    // {0x5A, 0x69, 0x67, 0x42, 0x65, 0x65, 0x41, 0x6C, 0x6C, 0x69, 0x61,
    //  0x6E, 0x63, 0x65, 0x30, 0x39} -> "ZigBeeAlliance09" unless overridden.
    static Status begin(Role role, uint16_t panId, uint8_t channel = 11);
    static void   end();
    static bool   isAvailable();

    // -- ZCL on/off cluster (M4 milestone) ---------------------------------

    // Send an ON or OFF command to a specific destination on cluster 0x0006.
    static Status sendOnOff(uint16_t destShortAddress, uint8_t endpoint, bool on);

    // Identity
    static void getEui64(uint8_t eui[8]);
};
