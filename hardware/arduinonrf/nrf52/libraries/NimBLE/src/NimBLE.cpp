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
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
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
bool g_pumpingEvents = false;
bool g_connParamUpdatePending = false;
bool g_connParamUpdateReady = false;
uint8_t g_connParamUpdateAttempts = 0;
uint8_t g_publicAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
char g_deviceName[31] = "nimble";
bool g_portReady = false;
uint8_t g_ownAddrType = BLE_OWN_ADDR_PUBLIC;

constexpr uint16_t kPreferredConnItvlMin = 12;   // 15 ms
constexpr uint16_t kPreferredConnItvlMax = 24;   // 30 ms
constexpr uint16_t kPreferredConnLatency = 0;
constexpr uint16_t kPreferredConnTimeout = 200;  // 2 s
constexpr uint8_t kMaxConnParamUpdateAttempts = 8;

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
NimBLE::ReceiveCallback g_rxCallback = nullptr;   // user RX handler (null = echo)

// Send a TX notification to the connected central. Shared by the RX echo path
// and the public NimBLE::write(). Returns the number of bytes queued (0 on any
// failure: no connection, notifications not enabled, or out of mbufs).
size_t nusNotify(const uint8_t *data, size_t length) {
    if (length == 0 || g_connHandle == BLE_HS_CONN_HANDLE_NONE || g_txValHandle == 0) {
        return 0;
    }
    if (length > sizeof(g_rxBuf)) {
        length = sizeof(g_rxBuf);
    }
    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, length);
    if (om == nullptr) {
        return 0;
    }
    if (ble_gatts_notify_custom(g_connHandle, g_txValHandle, om) != 0) {
        return 0;   // stack frees the mbuf on failure
    }
    g_notifyCount++;
    return length;
}

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
        if (g_rxCallback != nullptr) {
            // Hand the bytes to the sketch's onReceive() handler.
            g_rxCallback(g_rxBuf, len);
        } else {
            // No handler registered: echo the bytes back to the central.
            nusNotify(g_rxBuf, len);
        }
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

const struct ble_gatt_chr_def NUS_CHRS[] = {
    { &NUS_TX_UUID.u, nusAccessCb, nullptr, nullptr,
      BLE_GATT_CHR_F_NOTIFY, 0, &g_txValHandle },
    { &NUS_RX_UUID.u, nusAccessCb, nullptr, nullptr,
      BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP, 0, nullptr },
    { 0 },
};
const struct ble_gatt_svc_def NUS_SVCS[] = {
    { BLE_GATT_SVC_TYPE_PRIMARY, &NUS_SVC_UUID.u,  nullptr, NUS_CHRS },
    { 0 },
};

int registerGattServices() {
    int rc = ble_svc_gap_device_name_set(g_deviceName);
    if (rc != 0) return rc;

    rc = ble_svc_gap_device_appearance_set(BLE_SVC_GAP_APPEARANCE_GEN_UNKNOWN);
    if (rc != 0) return rc;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(NUS_SVCS);
    if (rc != 0) return rc;

    return ble_gatts_add_svcs(NUS_SVCS);
}

constexpr uint32_t FICR_BASE = 0x10000000UL;
constexpr uint32_t FICR_DEVICEADDRTYPE = 0x0A0UL;
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

bool ficrAddressIsRandomStatic() {
    return (reg32(FICR_BASE, FICR_DEVICEADDRTYPE) & 1U) != 0;
}

void pumpEvents() {
    if (!g_portReady || g_pumpingEvents) {
        return;
    }

    g_pumpingEvents = true;
    nimble_port_run();
    g_pumpingEvents = false;
}

NimBLE::Status startAdvertisingInternal();

__attribute__((used)) volatile uint32_t g_gap_dbg[4] = {0};  /* [0]=#events [1]=last type [2]=connect status [3]=conn_handle */
__attribute__((used)) volatile uint32_t g_poll_recover_dbg[4] = {0}; /* [0]=recoveries [1]=conn_find rc [2]=stale handle [3]=adv status */
__attribute__((used)) volatile uint32_t g_conn_params_dbg[12] = {0}; /* [0]=request count [1]=last conn_find rc [2]=last update rc [3]=update event count [4]=last update status [5]=current interval [6]=current latency [7]=current timeout [8]=requested min [9]=requested max [10]=pending flag [11]=attempt count */

void captureConnParams(uint16_t connHandle, uint32_t connFindRc) {
    g_conn_params_dbg[1] = connFindRc;
    if (connFindRc != 0) {
        return;
    }

    ble_gap_conn_desc desc;
    const int rc = ble_gap_conn_find(connHandle, &desc);
    g_conn_params_dbg[1] = (uint32_t)rc;
    if (rc != 0) {
        return;
    }

    g_conn_params_dbg[5] = desc.conn_itvl;
    g_conn_params_dbg[6] = desc.conn_latency;
    g_conn_params_dbg[7] = desc.supervision_timeout;
}

int requestFastConnectionParams(uint16_t connHandle) {
    ble_gap_upd_params params;
    memset(&params, 0, sizeof(params));

    params.itvl_min = kPreferredConnItvlMin;
    params.itvl_max = kPreferredConnItvlMax;
    params.latency = kPreferredConnLatency;
    params.supervision_timeout = kPreferredConnTimeout;

    ble_gap_conn_desc desc;
    const int findRc = ble_gap_conn_find(connHandle, &desc);
    g_conn_params_dbg[1] = (uint32_t)findRc;
    if (findRc == 0) {
        g_conn_params_dbg[5] = desc.conn_itvl;
        g_conn_params_dbg[6] = desc.conn_latency;
        g_conn_params_dbg[7] = desc.supervision_timeout;
        params.latency = desc.conn_latency;
        params.supervision_timeout = desc.supervision_timeout != 0
            ? desc.supervision_timeout
            : kPreferredConnTimeout;
    }

    g_conn_params_dbg[0]++;
    g_conn_params_dbg[8] = params.itvl_min;
    g_conn_params_dbg[9] = params.itvl_max;
    const int rc = ble_gap_update_params(connHandle, &params);
    g_conn_params_dbg[2] = (uint32_t)rc;
    return rc;
}

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
            ble_gap_conn_desc desc;
            const int descRc = ble_gap_conn_find(g_connHandle, &desc);
            g_connParamUpdatePending = (descRc == 0 && desc.role == BLE_GAP_ROLE_SLAVE);
            g_connParamUpdateReady = false;
            g_connParamUpdateAttempts = 0;
            g_conn_params_dbg[10] = g_connParamUpdatePending ? 1U : 0U;
            g_conn_params_dbg[11] = 0;
            captureConnParams(g_connHandle, 0);
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        /* BRING-UP: capture HCI disconnect reason (0x100|reason marks it a
         * disconnect). 0x213=remote-user-term, 0x208=supervision-timeout,
         * 0x216=conn-term-by-local-host, 0x23D=MIC-failure, 0x222=LMP-timeout. */
        g_gap_dbg[2] = 0x100u | (uint32_t)(uint16_t)event->disconnect.reason;
        g_connected = false;
        g_advertising = false;
        g_connHandle = BLE_HS_CONN_HANDLE_NONE;
        g_connParamUpdatePending = false;
        g_connParamUpdateReady = false;
        g_connParamUpdateAttempts = 0;
        g_conn_params_dbg[10] = 0;
        // Resume advertising so the link can be re-established (robustness).
        if (g_shouldAdvertise) {
            startAdvertisingInternal();
        }
        break;

    case BLE_GAP_EVENT_CONN_UPDATE:
        g_conn_params_dbg[3]++;
        g_conn_params_dbg[4] = (uint32_t)event->conn_update.status;
        if (event->conn_update.status == 0) {
            captureConnParams(event->conn_update.conn_handle, 0);
        }
        break;

    case BLE_GAP_EVENT_MTU:
        if (g_connParamUpdatePending) {
            g_connParamUpdateReady = true;
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

    uint8_t identityAddr[6];
    const bool randomStaticAddr = ficrAddressIsRandomStatic();
    loadPublicAddress(identityAddr);

    if (randomStaticAddr) {
        identityAddr[5] |= 0xC0U;
        ble_hs_id_set_rnd(identityAddr);
    }

    if (ble_hs_id_infer_auto(0, &g_ownAddrType) != 0) {
        g_ownAddrType = randomStaticAddr ? BLE_OWN_ADDR_RANDOM
                                         : BLE_OWN_ADDR_PUBLIC;
    }

    ble_hs_id_copy_addr(randomStaticAddr ? BLE_ADDR_RANDOM : BLE_ADDR_PUBLIC,
                        g_publicAddress, nullptr);

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
    advFields.uuids128 = &NUS_SVC_UUID;
    advFields.num_uuids128 = 1;
    advFields.uuids128_is_complete = 1;

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

extern "C" void nrfNimbleYieldPoll(void) {
    NimBLE::poll();
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
    if (deviceName != nullptr) {
        strncpy(g_deviceName, deviceName, sizeof(g_deviceName) - 1);
        g_deviceName[sizeof(g_deviceName) - 1] = '\0';
    }
    // Register standard GAP / GATT services plus the custom NUS echo service.
    // This must happen after ble_hs_init (done in nimble_port_init) and before
    // ble_hs_start (which calls ble_gatts_start).
    registerGattServices();
    g_portReady = nimble_port_get_dflt_eventq() != nullptr;
    if (!g_portReady) {
        return NIMBLE_INTERNAL;
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

    if (!g_started || !g_synced) {
        return;
    }

    if (g_connected && g_connHandle != BLE_HS_CONN_HANDLE_NONE) {
        const int rc = ble_gap_conn_find(g_connHandle, nullptr);
        if (rc != 0) {
            g_poll_recover_dbg[0]++;
            g_poll_recover_dbg[1] = (uint32_t)rc;
            g_poll_recover_dbg[2] = g_connHandle;
            g_connected = false;
            g_advertising = false;
            g_connHandle = BLE_HS_CONN_HANDLE_NONE;
            g_connParamUpdatePending = false;
            g_connParamUpdateReady = false;
            g_connParamUpdateAttempts = 0;
            g_conn_params_dbg[10] = 0;
        } else if (g_connParamUpdatePending && g_connParamUpdateReady &&
                   g_connParamUpdateAttempts < kMaxConnParamUpdateAttempts) {
            g_connParamUpdateAttempts++;
            g_conn_params_dbg[11] = g_connParamUpdateAttempts;
            const int updateRc = requestFastConnectionParams(g_connHandle);
            if (updateRc == 0 || updateRc == BLE_HS_EALREADY) {
                g_connParamUpdatePending = false;
                g_connParamUpdateReady = false;
                g_conn_params_dbg[10] = 0;
            } else if (updateRc != BLE_HS_EBUSY) {
                g_connParamUpdatePending = false;
                g_connParamUpdateReady = false;
                g_conn_params_dbg[10] = 0;
            }
        }
    }

    if (g_shouldAdvertise && !g_connected && !g_advertising) {
        g_poll_recover_dbg[3] = (uint32_t)startAdvertisingInternal();
    }
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

size_t NimBLE::write(const uint8_t *data, size_t length) {
#if NRF_NIMBLE_PORTING_VENDORED
    poll();
    if (data == nullptr) {
        return 0;
    }
    return nusNotify(data, length);
#else
    (void)data; (void)length;
    return 0;
#endif
}

size_t NimBLE::write(const char *text) {
    if (text == nullptr) {
        return 0;
    }
    return write(reinterpret_cast<const uint8_t *>(text), strlen(text));
}

void NimBLE::onReceive(ReceiveCallback callback) {
    g_rxCallback = callback;
}
void           NimBLE::getPublicAddress(uint8_t addr[6]) {
    poll();
    if (!g_started) {
        memset(addr, 0xFF, 6);
        return;
    }
    memcpy(addr, g_publicAddress, sizeof(g_publicAddress));
}
