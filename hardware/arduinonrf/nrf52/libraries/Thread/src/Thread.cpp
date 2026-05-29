// Thread.cpp - implementation. At M1 status (see
// docs/platform/THREAD_INTEGRATION_PLAN.md):
//   * The OpenThread public API headers (openthread/error.h,
//     openthread/instance.h) and the full platform abstraction surface
//     (openthread/platform/*.h) are VENDORED into src/.
//   * The actual OpenThread core (link layer, MAC, NWK, MeshCoP, CoAP)
//     and the nrf-802154 PHY driver are NOT YET vendored. begin() still
//     returns THREAD_NOT_VENDORED, but the smoke test confirms the
//     header tree is linkable. M2 lands the actual stack init.

#include "Thread.h"
#include <string.h>

#if __has_include("openthread/instance.h")
#define NRF_THREAD_HEADERS_VENDORED 1
extern "C" {
#include "openthread/error.h"
#include "openthread/instance.h"
}
#else
#define NRF_THREAD_HEADERS_VENDORED 0
#endif

namespace { bool g_started = false; }

Thread::Status Thread::begin(Role role) {
#if NRF_THREAD_HEADERS_VENDORED
    // Sanity probe: reference the OpenThread error enum. If the headers
    // are present and structurally usable this trivially evaluates.
    volatile otError sanity = OT_ERROR_NONE;
    (void)sanity;
    (void)role;
    g_started = true;
    return THREAD_NOT_VENDORED;   // headers OK; core + radio is M2
#else
    (void)role;
    return THREAD_NOT_VENDORED;
#endif
}

void Thread::end() { g_started = false; }

bool Thread::isAvailable() {
    return false;   // turns true at M3 when the device is attached
}

Thread::Status Thread::joinNetwork(const char *, const uint8_t [8],
                                     const uint8_t [16], uint8_t, uint16_t) {
    return THREAD_NOT_VENDORED;
}

bool Thread::isAttached() { return false; }

Thread::Status Thread::onCoapGet(const char *, coapHandler_t) {
    return THREAD_NOT_VENDORED;
}

void Thread::getEui64(uint8_t eui[8]) { memset(eui, 0xFFU, 8); }
