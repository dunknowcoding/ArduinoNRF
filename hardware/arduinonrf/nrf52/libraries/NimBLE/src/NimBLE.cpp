// NimBLE.cpp - stub implementation. Returns NIMBLE_NOT_VENDORED on every op
// until the Apache Mynewt NimBLE source tree is dropped under vendor/.
// See vendor/README.md.

#include "NimBLE.h"
#include <string.h>

#ifndef NRF_NIMBLE_VENDORED
#define NRF_NIMBLE_VENDORED 0
#endif

#if NRF_NIMBLE_VENDORED
// Real wiring lives in vendor_wiring.cpp once the source is present.
extern "C" {
extern NimBLE::Status nrfNimble_begin(const char *deviceName);
extern void           nrfNimble_end();
extern NimBLE::Status nrfNimble_startAdvertising();
extern NimBLE::Status nrfNimble_stopAdvertising();
extern bool           nrfNimble_isConnected();
extern int            nrfNimble_connectionCount();
extern void           nrfNimble_getPublicAddress(uint8_t addr[6]);
}
#endif

namespace {
bool g_started = false;
}

NimBLE::Status NimBLE::begin(const char *deviceName) {
#if NRF_NIMBLE_VENDORED
    if (deviceName == nullptr) return NIMBLE_BAD_PARAM;
    const Status r = nrfNimble_begin(deviceName);
    g_started = (r == NIMBLE_OK);
    return r;
#else
    (void)deviceName;
    return NIMBLE_NOT_VENDORED;
#endif
}

void NimBLE::end() {
#if NRF_NIMBLE_VENDORED
    if (g_started) {
        nrfNimble_end();
        g_started = false;
    }
#endif
}

bool NimBLE::isAvailable() {
#if NRF_NIMBLE_VENDORED
    return g_started;
#else
    return false;
#endif
}

#if NRF_NIMBLE_VENDORED

NimBLE::Status NimBLE::startAdvertising() {
    if (!g_started) return NIMBLE_NOT_STARTED;
    return nrfNimble_startAdvertising();
}

NimBLE::Status NimBLE::stopAdvertising() {
    if (!g_started) return NIMBLE_NOT_STARTED;
    return nrfNimble_stopAdvertising();
}

bool NimBLE::isConnected() {
    return g_started && nrfNimble_isConnected();
}

int NimBLE::connectionCount() {
    if (!g_started) return 0;
    return nrfNimble_connectionCount();
}

void NimBLE::getPublicAddress(uint8_t addr[6]) {
    if (!g_started) { memset(addr, 0xFFU, 6); return; }
    nrfNimble_getPublicAddress(addr);
}

#else  // NRF_NIMBLE_VENDORED == 0

NimBLE::Status NimBLE::startAdvertising()  { return NIMBLE_NOT_VENDORED; }
NimBLE::Status NimBLE::stopAdvertising()   { return NIMBLE_NOT_VENDORED; }
bool           NimBLE::isConnected()       { return false; }
int            NimBLE::connectionCount()   { return 0; }
void           NimBLE::getPublicAddress(uint8_t addr[6]) { memset(addr, 0xFFU, 6); }

#endif
