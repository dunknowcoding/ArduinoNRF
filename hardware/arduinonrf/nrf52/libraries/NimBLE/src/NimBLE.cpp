// NimBLE.cpp - implementation. At M1 status (see
// docs/platform/NIMBLE_INTEGRATION_PLAN.md):
//   * The Apache Mynewt porting layer (sys/queue, mempool, mbuf, cputime,
//     endian) is VENDORED into src/.
//   * The bare-metal NPL (event queue, critical section, mutex/sem) is
//     IMPLEMENTED in src/nimble_src/npl_os_bare.c with PRIMASK-guarded
//     intrusive lists, and begin() now enters through the standard
//     NimBLE port layer in src/nimble_src/nimble_port_bare.c.
//   * A first upstream host + controller bootstrap slice is vendored into
//     src/. The Arduino-facing API now cooperatively pumps the default NimBLE
//     event queue and can attempt basic GAP advertising through the native
//     host/controller path.

#include "NimBLE.h"

#include "NrfClock.h"

#if __has_include("nimble/nimble_npl.h")
#define NRF_NIMBLE_PORTING_VENDORED 1
extern "C" {
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "host/ble_hs_id.h"
#include "nimble/nimble_npl.h"
#include "nimble/nimble_port.h"
#include "nimble/transport.h"
#include "nimble/transport_impl.h"
}
#else
#define NRF_NIMBLE_PORTING_VENDORED 0
#endif

#include <string.h>

// SWD-readable diagnostics for the host bring-up (serial CDC capture is flaky
// on this host; read these by symbol address via J-Link after begin()).
extern "C" {
__attribute__((used)) volatile int      g_nimble_dbg_startrc = 0x7F;  // ble_hs_start() return
__attribute__((used)) volatile uint32_t g_nimble_dbg_synced  = 0;     // g_synced after pump
}

namespace {
bool g_started = false;
bool g_advertising = false;
bool g_connected = false;
bool g_synced = false;
bool g_shouldAdvertise = false;
uint8_t g_publicAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
char g_deviceName[31] = "nimble";
bool g_portReady = false;
uint8_t g_ownAddrType = BLE_OWN_ADDR_PUBLIC;

constexpr uint32_t FICR_BASE = 0x10000000UL;
constexpr uint32_t FICR_DEVICEADDR0 = 0x0A4UL;
constexpr uint32_t FICR_DEVICEADDR1 = 0x0A8UL;

inline volatile uint32_t &reg32(uint32_t base, uint32_t offset) {
    return *reinterpret_cast<volatile uint32_t *>(base + offset);
}

void loadPublicAddress(uint8_t addr[6]) {
    const uint32_t low = reg32(FICR_BASE, FICR_DEVICEADDR0);
    const uint32_t high = reg32(FICR_BASE, FICR_DEVICEADDR1);

    addr[0] = static_cast<uint8_t>(low & 0xFFU);
    addr[1] = static_cast<uint8_t>((low >> 8) & 0xFFU);
    addr[2] = static_cast<uint8_t>((low >> 16) & 0xFFU);
    addr[3] = static_cast<uint8_t>((low >> 24) & 0xFFU);
    addr[4] = static_cast<uint8_t>(high & 0xFFU);
    addr[5] = static_cast<uint8_t>((high >> 8) & 0xFFU);
}

void pumpEvents() {
    if (g_portReady) {
        nimble_port_run();
    }
}

NimBLE::Status startAdvertisingInternal();

int handleGapEvent(struct ble_gap_event *event, void *arg) {
    (void)arg;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        g_connected = (event->connect.status == 0);
        g_advertising = false;
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        g_connected = false;
        g_advertising = false;
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        g_advertising = false;
        break;

    default:
        break;
    }

    return 0;
}

void handleHostReset(int reason) {
    (void)reason;
    g_synced = false;
    g_connected = false;
    g_advertising = false;
}

void handleHostSync() {
    g_synced = true;

    if (ble_hs_id_infer_auto(0, &g_ownAddrType) != 0) {
        g_ownAddrType = BLE_OWN_ADDR_PUBLIC;
    }

    ble_hs_id_copy_addr(BLE_ADDR_PUBLIC, g_publicAddress, nullptr);

    if (g_shouldAdvertise) {
        startAdvertisingInternal();
    }
}

NimBLE::Status startAdvertisingInternal() {
    if (!g_synced) {
        return NimBLE::NIMBLE_INTERNAL;
    }

    if (g_advertising) {
        return NimBLE::NIMBLE_OK;
    }

    ble_gap_adv_params advParams;
    ble_hs_adv_fields advFields;
    ble_hs_adv_fields rspFields;
    memset(&advParams, 0, sizeof(advParams));
    memset(&advFields, 0, sizeof(advFields));
    memset(&rspFields, 0, sizeof(rspFields));

    advParams.conn_mode = BLE_GAP_CONN_MODE_UND;
    advParams.disc_mode = BLE_GAP_DISC_MODE_GEN;

    advFields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    rspFields.name = reinterpret_cast<const uint8_t *>(g_deviceName);
    rspFields.name_len = strlen(g_deviceName);
    rspFields.name_is_complete = 1;

    if (ble_gap_adv_set_fields(&advFields) != 0) {
        return NimBLE::NIMBLE_INTERNAL;
    }

    if (ble_gap_adv_rsp_set_fields(&rspFields) != 0) {
        return NimBLE::NIMBLE_INTERNAL;
    }

    if (ble_gap_adv_start(g_ownAddrType, nullptr, BLE_HS_FOREVER, &advParams,
                          handleGapEvent, nullptr) != 0) {
        return NimBLE::NIMBLE_INTERNAL;
    }

    g_advertising = true;
    return NimBLE::NIMBLE_OK;
}
}

NimBLE::Status NimBLE::begin(const char *deviceName) {
#if NRF_NIMBLE_PORTING_VENDORED
    if (g_started) {
        poll();
        return g_synced ? NIMBLE_OK : NIMBLE_INTERNAL;
    }

    nrfStartHfclk();
    nrfStartLfclk();   // RTC-backed os_cputime / hal_timer + LL scheduler need LFCLK
    // Full port bring-up: eventq + mempools + LL/PHY + timer backend + transport
    // + host (ble_hs_init). Sets up the controller so the host can actually sync.
    nimble_port_init();
    // Set host callbacks AFTER init (ble_hs_init may reset ble_hs_cfg).
    ble_hs_cfg.reset_cb = handleHostReset;
    ble_hs_cfg.sync_cb = handleHostSync;
    g_portReady = nimble_port_get_dflt_eventq() != nullptr;
    if (!g_portReady) {
        return NIMBLE_INTERNAL;
    }
    if (deviceName != nullptr) {
        strncpy(g_deviceName, deviceName, sizeof(g_deviceName) - 1);
        g_deviceName[sizeof(g_deviceName) - 1] = '\0';
    }
    g_connected = false;
    g_synced = false;
    g_shouldAdvertise = false;
    loadPublicAddress(g_publicAddress);
    g_started = true;

    // Start the host. The deferred path (ble_hs_sched_start -> stage1 -> stage2
    // -> ble_hs_start) was never scheduled, so the host never started. Call
    // ble_hs_start() directly; on the controller's HCI reset command-complete
    // this fires sync_cb (handleHostSync). Pump the event queue a bounded
    // number of times so the startup sequence and any HCI round-trips run.
    g_nimble_dbg_startrc = ble_hs_start();
    for (int i = 0; i < 64 && !g_synced; ++i) {
        poll();
    }
    g_nimble_dbg_synced = g_synced ? 1U : 0U;
    return g_synced ? NIMBLE_OK : NIMBLE_INTERNAL;
#else
    return NIMBLE_NOT_VENDORED;
#endif
}

void NimBLE::end() {
    poll();
    if (g_advertising) {
        ble_gap_adv_stop();
    }
    g_portReady = false;
    g_started = false;
    g_advertising = false;
    g_connected = false;
    g_synced = false;
    g_shouldAdvertise = false;
    memset(g_publicAddress, 0xFF, sizeof(g_publicAddress));
}

bool NimBLE::isAvailable() {
    poll();
    return g_started && g_synced;
}

void NimBLE::poll() {
    pumpEvents();
}

NimBLE::Status NimBLE::startAdvertising() {
    if (!g_started) {
        return NIMBLE_NOT_STARTED;
    }

    g_shouldAdvertise = true;
    poll();
    return startAdvertisingInternal();
}

NimBLE::Status NimBLE::stopAdvertising()  {
    if (!g_started) {
        return NIMBLE_NOT_STARTED;
    }
    poll();
    g_shouldAdvertise = false;
    if (g_advertising) {
        if (ble_gap_adv_stop() != 0) {
            return NIMBLE_INTERNAL;
        }
        g_advertising = false;
    }
    return NIMBLE_OK;
}
bool           NimBLE::isConnected()      { poll(); return g_connected; }
int            NimBLE::connectionCount()  { poll(); return g_connected ? 1 : 0; }
void           NimBLE::getPublicAddress(uint8_t addr[6]) {
    poll();
    if (!g_started) {
        memset(addr, 0xFF, 6);
        return;
    }
    memcpy(addr, g_publicAddress, sizeof(g_publicAddress));
}
