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
#include "host/ble_gatt.h"
#include "host/ble_att.h"
#include "host/ble_uuid.h"
#include "os/os_mbuf.h"
#include "host/ble_hs.h"
#include "host/ble_hs_id.h"
#include "host/ble_hs_mbuf.h"
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
__attribute__((used)) volatile int      g_dbg_adv_step = 0;           // adv: 1=setfields 2=rsp 3=start 9=ok
__attribute__((used)) volatile int      g_dbg_adv_rc   = 0;           // adv: failing gap rc
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

// --- Nordic UART Service (NUS) echo GATT server -----------------------------
// A connection/data test surface: the central writes to RX, the board echoes
// the bytes back as a TX notification. UUIDs are the de-facto "BLE UART" ones,
// so off-the-shelf scanners (nRF Connect, bleak) can drive it. Counters are
// SWD-readable for hardware verification.
uint16_t g_txValHandle = 0;
uint16_t g_connHandle = BLE_HS_CONN_HANDLE_NONE;
__attribute__((used)) volatile uint32_t g_rxCount = 0;       // writes received
__attribute__((used)) volatile uint32_t g_lastRxLen = 0;     // bytes in last write
__attribute__((used)) volatile uint32_t g_notifyCount = 0;   // notifications sent
__attribute__((used)) volatile uint32_t g_lastConnHandle = 0xFFFF;
uint8_t g_rxBuf[244];

// 6E40000x-B5A3-F393-E0A9-E50E24DCCA9E, bytes LSB-first.
const ble_uuid128_t NUS_SVC_UUID = BLE_UUID128_INIT(
    0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,0x93,0xf3,0xa3,0xb5,0x01,0x00,0x40,0x6e);
const ble_uuid128_t NUS_RX_UUID = BLE_UUID128_INIT(
    0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,0x93,0xf3,0xa3,0xb5,0x02,0x00,0x40,0x6e);
const ble_uuid128_t NUS_TX_UUID = BLE_UUID128_INIT(
    0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,0x93,0xf3,0xa3,0xb5,0x03,0x00,0x40,0x6e);

int nusAccessCb(uint16_t conn_handle, uint16_t attr_handle,
                struct ble_gatt_access_ctxt *ctxt, void *arg) {
    (void)conn_handle; (void)attr_handle; (void)arg;
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint16_t len = 0;
        ble_hs_mbuf_to_flat(ctxt->om, g_rxBuf, sizeof(g_rxBuf), &len);
        g_lastRxLen = len;
        g_rxCount++;
        // Echo the bytes back to the central as a TX notification.
        if (g_connHandle != BLE_HS_CONN_HANDLE_NONE && g_txValHandle != 0) {
            struct os_mbuf *om = ble_hs_mbuf_from_flat(g_rxBuf, len);
            if (om != nullptr &&
                ble_gatts_notify_custom(g_connHandle, g_txValHandle, om) == 0) {
                g_notifyCount++;
            }
        }
    }
    return 0;
}

const struct ble_gatt_chr_def NUS_CHRS[] = {
    { &NUS_RX_UUID.u, nusAccessCb, nullptr, nullptr,
      BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP, 0, nullptr },
    { &NUS_TX_UUID.u, nusAccessCb, nullptr, nullptr,
      BLE_GATT_CHR_F_NOTIFY, 0, &g_txValHandle },
    { 0 },
};
// Standard GAP (0x1800) + GATT (0x1801) services. The base ble_svc_gap/gatt
// modules aren't vendored, so register them by hand. Many centrals (incl.
// Windows/bleak) probe the GAP service (Device Name) during discovery via a
// Find-By-Type-Value before continuing; without it, discovery stalls.
const ble_uuid16_t GAP_SVC_UUID      = BLE_UUID16_INIT(0x1800);
const ble_uuid16_t GAP_DEVNAME_UUID  = BLE_UUID16_INIT(0x2A00);
const ble_uuid16_t GAP_APPEAR_UUID   = BLE_UUID16_INIT(0x2A01);
const ble_uuid16_t GATT_SVC_UUID     = BLE_UUID16_INIT(0x1801);
const ble_uuid16_t GATT_SVC_CHG_UUID = BLE_UUID16_INIT(0x2A05);

int gapAccessCb(uint16_t conn_handle, uint16_t attr_handle,
                struct ble_gatt_access_ctxt *ctxt, void *arg) {
    (void)conn_handle; (void)attr_handle; (void)arg;
    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    uint16_t uuid = ble_uuid_u16(ctxt->chr->uuid);
    if (uuid == 0x2A00) {   // Device Name
        return os_mbuf_append(ctxt->om, g_deviceName, strlen(g_deviceName)) == 0
                   ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    if (uuid == 0x2A01) {   // Appearance (0 = unknown)
        uint16_t appearance = 0;
        return os_mbuf_append(ctxt->om, &appearance, sizeof(appearance)) == 0
                   ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

const struct ble_gatt_chr_def GAP_CHRS[] = {
    { &GAP_DEVNAME_UUID.u, gapAccessCb, nullptr, nullptr, BLE_GATT_CHR_F_READ, 0, nullptr },
    { &GAP_APPEAR_UUID.u,  gapAccessCb, nullptr, nullptr, BLE_GATT_CHR_F_READ, 0, nullptr },
    { 0 },
};
const struct ble_gatt_chr_def GATT_CHRS[] = {
    { &GATT_SVC_CHG_UUID.u, gapAccessCb, nullptr, nullptr, BLE_GATT_CHR_F_INDICATE, 0, nullptr },
    { 0 },
};
const struct ble_gatt_svc_def ALL_SVCS[] = {
    { BLE_GATT_SVC_TYPE_PRIMARY, &GAP_SVC_UUID.u,  nullptr, GAP_CHRS },
    { BLE_GATT_SVC_TYPE_PRIMARY, &GATT_SVC_UUID.u, nullptr, GATT_CHRS },
    { BLE_GATT_SVC_TYPE_PRIMARY, &NUS_SVC_UUID.u,  nullptr, NUS_CHRS },
    { 0 },
};

int registerGattServices() {
    int rc = ble_gatts_count_cfg(ALL_SVCS);
    if (rc != 0) return rc;
    return ble_gatts_add_svcs(ALL_SVCS);
}

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

__attribute__((used)) volatile uint32_t g_gap_dbg[4] = {0};  /* [0]=#events [1]=last type [2]=connect status [3]=conn_handle */

int handleGapEvent(struct ble_gap_event *event, void *arg) {
    (void)arg;
    g_gap_dbg[0]++;
    g_gap_dbg[1] = event->type;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        g_gap_dbg[2] = (uint32_t)event->connect.status;
        g_gap_dbg[3] = event->connect.conn_handle;
        g_connected = (event->connect.status == 0);
        g_advertising = false;
        if (g_connected) {
            g_connHandle = event->connect.conn_handle;
            g_lastConnHandle = event->connect.conn_handle;
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        g_connected = false;
        g_advertising = false;
        g_connHandle = BLE_HS_CONN_HANDLE_NONE;
        // Resume advertising so the link can be re-established (robustness).
        if (g_shouldAdvertise) {
            startAdvertisingInternal();
        }
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

    // The nRF52840 has no IEEE public address, so advertising with
    // own_addr_type=PUBLIC fails with BLE_HS_ENOADDR (21). Program a random
    // STATIC address from the factory FICR DEVICEADDR (top 2 bits = 0b11) and
    // advertise as RANDOM.
    uint8_t rnd[6];
    loadPublicAddress(rnd);
    rnd[5] |= 0xC0;
    ble_hs_id_set_rnd(rnd);

    if (ble_hs_id_infer_auto(0, &g_ownAddrType) != 0) {
        g_ownAddrType = BLE_OWN_ADDR_RANDOM;
    }

    ble_hs_id_copy_addr(BLE_ADDR_RANDOM, g_publicAddress, nullptr);

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

    int advrc;
    g_dbg_adv_step = 1;
    advrc = ble_gap_adv_set_fields(&advFields);
    if (advrc != 0) { g_dbg_adv_rc = advrc; return NimBLE::NIMBLE_INTERNAL; }

    g_dbg_adv_step = 2;
    advrc = ble_gap_adv_rsp_set_fields(&rspFields);
    if (advrc != 0) { g_dbg_adv_rc = advrc; return NimBLE::NIMBLE_INTERNAL; }

    g_dbg_adv_step = 3;
    advrc = ble_gap_adv_start(g_ownAddrType, nullptr, BLE_HS_FOREVER, &advParams,
                              handleGapEvent, nullptr);
    if (advrc != 0) { g_dbg_adv_rc = advrc; return NimBLE::NIMBLE_INTERNAL; }

    g_dbg_adv_step = 9;
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
    // CRITICAL: re-point the host event queue at the (now-valid) default queue.
    // ble_hs_init() ran inside nimble_port_init() and called
    // ble_hs_evq_set(nimble_port_get_dflt_eventq()) - but at that point
    // g_port_initialized was still false, so the getter returned NULL and
    // ble_hs_evq was left NULL. With a NULL host evq, every ASYNC host event
    // (LE Connection Complete, Disconnect, etc.) is silently dropped by
    // ble_hs_enqueue_hci_event -> the host never delivers BLE_GAP_EVENT_CONNECT.
    // nimble_port_init() has completed here, so the getter now returns the real
    // (poll-pumped) queue. (Hardware-verified: without this, the controller
    // established the link and sent the conn-complete but the host never saw it.)
    ble_hs_evq_set(nimble_port_get_dflt_eventq());
    // Set host callbacks AFTER init (ble_hs_init may reset ble_hs_cfg).
    ble_hs_cfg.reset_cb = handleHostReset;
    ble_hs_cfg.sync_cb = handleHostSync;
    // Register the NUS echo GATT service. Must happen after ble_hs_init (done in
    // nimble_port_init) and before ble_hs_start (which calls ble_gatts_start).
    registerGattServices();
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
