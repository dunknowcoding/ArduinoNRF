// Zigbee.cpp - stub implementation.
#include "Zigbee.h"
#include <string.h>

#ifndef NRF_ZIGBEE_VENDORED
#define NRF_ZIGBEE_VENDORED 0
#endif

#if NRF_ZIGBEE_VENDORED
extern "C" {
extern Zigbee::Status nrfZigbee_begin(uint8_t role, uint16_t panId, uint8_t channel);
extern void            nrfZigbee_end();
extern Zigbee::Status nrfZigbee_sendOnOff(uint16_t dest, uint8_t ep, bool on);
extern void            nrfZigbee_getEui64(uint8_t eui[8]);
}
#endif

namespace { bool g_started = false; }

Zigbee::Status Zigbee::begin(Role role, uint16_t panId, uint8_t channel) {
#if NRF_ZIGBEE_VENDORED
    const Status r = nrfZigbee_begin(static_cast<uint8_t>(role), panId, channel);
    g_started = (r == ZIGBEE_OK);
    return r;
#else
    (void)role; (void)panId; (void)channel;
    return ZIGBEE_NOT_VENDORED;
#endif
}

void Zigbee::end() {
#if NRF_ZIGBEE_VENDORED
    if (g_started) { nrfZigbee_end(); g_started = false; }
#endif
}

bool Zigbee::isAvailable() {
#if NRF_ZIGBEE_VENDORED
    return g_started;
#else
    return false;
#endif
}

Zigbee::Status Zigbee::sendOnOff(uint16_t dest, uint8_t ep, bool on) {
#if NRF_ZIGBEE_VENDORED
    if (!g_started) return ZIGBEE_NOT_STARTED;
    return nrfZigbee_sendOnOff(dest, ep, on);
#else
    (void)dest; (void)ep; (void)on;
    return ZIGBEE_NOT_VENDORED;
#endif
}

void Zigbee::getEui64(uint8_t eui[8]) {
#if NRF_ZIGBEE_VENDORED
    if (g_started) { nrfZigbee_getEui64(eui); return; }
#endif
    memset(eui, 0xFFU, 8);
}
