// nimble_port_bare.c - cooperative bare-metal implementation of the standard
// Apache NimBLE port entrypoints.
//
// nimble_port_init() now mirrors the upstream porting/nimble/src/nimble_port.c
// sequence so the controller (LL + PHY + timer backend) is actually brought
// up, not just the host. nimble_port_run() stays NON-BLOCKING (drains the
// default event queue then returns) so the Arduino sketch can cooperatively
// pump it from loop()/poll() instead of dedicating a task to it.

#include "syscfg/syscfg.h"
#include "os/os.h"
#include "os/os_mempool.h"
#include "os/os_mbuf.h"
#include "os/os_cputime.h"
#include "hal/hal_timer.h"
#include "nimble/nimble_port.h"
#include "nimble/transport.h"

#if NIMBLE_CFG_CONTROLLER
#include "controller/ble_ll.h"
#endif

void os_msys_init(void);
void os_mempool_module_init(void);

static struct ble_npl_eventq g_default_eventq;
static bool g_port_initialized = false;

void nimble_port_init(void) {
    if (g_port_initialized) {
        return;
    }

    /* Default event queue (host parent task runs on this). */
    ble_npl_eventq_init(&g_default_eventq);

    /* Memory pools + system mbuf pool. */
    os_mempool_module_init();
    os_msys_init();

#if NIMBLE_CFG_CONTROLLER
    /* Link layer (also brings up the PHY via SYSINIT-equivalent inside). */
    ble_ll_init();
#endif

    /* Transport + host. */
    ble_transport_init();
    ble_transport_hs_init();

#if NIMBLE_CFG_CONTROLLER
    /* Timer backend the LL scheduler + os_cputime ride on. hal_timer "5" is
     * the cputimer instance; os_cputime runs it at 32768 Hz. */
    hal_timer_init(5, NULL);
    os_cputime_init(32768);
#endif

    /* Controller transport (tells the host the controller is ready). */
    ble_transport_ll_init();

    g_port_initialized = true;
}

void nimble_port_run(void) {
    if (!g_port_initialized) {
        return;
    }

    /* Non-blocking drain of BOTH event queues, then return so the cooperative
     * caller (NimBLE::poll) keeps control. The controller LL task has its own
     * queue (g_ble_ll_data.ll_evq) separate from the host's default queue; both
     * must be serviced for the host<->controller HCI handshake (and all
     * deferred LL work) to progress. Real-time radio timing runs in the timer /
     * RADIO ISRs, independent of this pump. */
    for (;;) {
        bool did_work = false;
        struct ble_npl_event *ev = ble_npl_eventq_get(&g_default_eventq, 0);
        if (ev != NULL) {
            ble_npl_event_run(ev);
            did_work = true;
        }
#if NIMBLE_CFG_CONTROLLER
        ev = ble_npl_eventq_get(&g_ble_ll_data.ll_evq, 0);
        if (ev != NULL) {
            ble_npl_event_run(ev);
            did_work = true;
        }
#endif
        if (!did_work) {
            return;
        }
    }
}

struct ble_npl_eventq *nimble_port_get_dflt_eventq(void) {
    if (!g_port_initialized) {
        return NULL;
    }

    return &g_default_eventq;
}

#if NIMBLE_CFG_CONTROLLER
void nimble_port_ll_task_func(void *arg) {
    (void)arg;
}
#endif
