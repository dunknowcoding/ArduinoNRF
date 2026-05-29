// NimBLE.cpp - implementation. At M1 status (see
// docs/platform/NIMBLE_INTEGRATION_PLAN.md):
//   * The Apache Mynewt porting layer (sys/queue, mempool, mbuf, cputime,
//     endian) is VENDORED into src/.
//   * The bare-metal NPL (event queue, critical section, mutex/sem) is
//     IMPLEMENTED in src/nimble_src/npl_os_bare.c with PRIMASK-guarded
//     intrusive lists.
//   * The actual BLE host (ATT/GATT/SMP/L2CAP) and controller (link layer)
//     are NOT YET vendored. begin() therefore still returns
//     NIMBLE_NOT_VENDORED, but the porting + NPL code DOES compile and
//     link into every sketch using this library, paving the way for M2
//     to add the host/controller and flip NIMBLE_NOT_VENDORED to
//     NIMBLE_OK.

#include "NimBLE.h"

#if __has_include("nimble/nimble_npl.h")
#define NRF_NIMBLE_PORTING_VENDORED 1
extern "C" {
#include "nimble/nimble_npl.h"
}
#else
#define NRF_NIMBLE_PORTING_VENDORED 0
#endif

#include <string.h>

namespace { bool g_started = false; }

NimBLE::Status NimBLE::begin(const char *deviceName) {
    (void)deviceName;
#if NRF_NIMBLE_PORTING_VENDORED
    // Sanity-check the porting layer: create + use an event queue. If this
    // works the NPL is correctly linked.
    static struct ble_npl_eventq smoke_q;
    ble_npl_eventq_init(&smoke_q);
    // M2 will call ble_hs_init() + ble_ll_init() here. For now, just confirm
    // the porting layer is alive.
    g_started = true;
    return NIMBLE_NOT_VENDORED;   // porting works; host/controller is M2
#else
    return NIMBLE_NOT_VENDORED;
#endif
}

void NimBLE::end() {
    g_started = false;
}

bool NimBLE::isAvailable() {
    return false;   // turns true at M3 when actual GAP/GATT is up
}

NimBLE::Status NimBLE::startAdvertising() { return NIMBLE_NOT_VENDORED; }
NimBLE::Status NimBLE::stopAdvertising()  { return NIMBLE_NOT_VENDORED; }
bool           NimBLE::isConnected()      { return false; }
int            NimBLE::connectionCount()  { return 0; }
void           NimBLE::getPublicAddress(uint8_t addr[6]) { memset(addr, 0xFF, 6); }
