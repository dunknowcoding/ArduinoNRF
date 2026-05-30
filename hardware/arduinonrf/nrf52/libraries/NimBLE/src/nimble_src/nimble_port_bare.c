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

// SWD-readable progress marker: set just before each init step so a J-Link read
// after a fault shows which call did NOT return. ALSO mirrored to POWER.GPREGRET2
// (0x40000520), a RETAINED register that survives the fault->bootloader reset
// the core's fault handler performs - so the last stage is still readable even
// after the board bounces back into the bootloader (normal .bss RAM gets
// clobbered by the bootloader). (Bring-up debug.)
__attribute__((used)) volatile uint32_t g_nimble_port_stage = 0;
#define NIMBLE_DBG_STAGE(n) do { \
    g_nimble_port_stage = (uint32_t)(n); \
    *(volatile uint32_t *)0x40000520UL = (uint32_t)(n); \
} while (0)

/* RAM vector table. The controller (ble_phy.c / ble_hw.c) installs its RADIO,
 * CCM_AAR and RNG ISRs via NVIC_SetVector(), which writes to the table at
 * SCB->VTOR. On this core VTOR points at the FLASH app table (0x1000), so those
 * writes are silently dropped (flash is read-only). The IRQ then dispatches
 * through the flash vector - fine for RADIO/RNG (the bare port defines strong
 * forwarders) but CCM_AAR has none, so it lands in Default_Handler which RESETS
 * the chip. That is the "host never syncs / board bounces to bootloader during
 * ble_hs_start" failure. Relocating the table to RAM up front makes every
 * NVIC_SetVector() stick, so all controller IRQs reach the right ISR.
 * 64 entries (16 system + 48 peripheral), 256-byte aligned per Cortex-M VTOR. */
static uint32_t g_ram_vectors[64] __attribute__((aligned(256)));

static void nimble_relocate_vectors_to_ram(void) {
    volatile uint32_t *vtor = (volatile uint32_t *)0xE000ED08UL;
    if (*vtor == (uint32_t)g_ram_vectors) {
        return;   /* already relocated */
    }
    const uint32_t *src = (const uint32_t *)(*vtor);
    uint32_t ctx = ble_npl_hw_enter_critical();
    for (int i = 0; i < 64; ++i) {
        g_ram_vectors[i] = src[i];
    }
    __asm volatile("dsb" ::: "memory");
    *vtor = (uint32_t)g_ram_vectors;
    __asm volatile("dsb\n isb" ::: "memory");
    ble_npl_hw_exit_critical(ctx);
}

void nimble_port_init(void) {
    if (g_port_initialized) {
        return;
    }

    /* Must run BEFORE ble_ll_init()/ble_phy_init() (stage 4) so their
     * NVIC_SetVector() calls land in a writable (RAM) vector table. */
    NIMBLE_DBG_STAGE(0);
    nimble_relocate_vectors_to_ram();

    /* Default event queue (host parent task runs on this). */
    NIMBLE_DBG_STAGE(1);
    ble_npl_eventq_init(&g_default_eventq);

    /* Memory pools + system mbuf pool. */
    NIMBLE_DBG_STAGE(2);
    os_mempool_module_init();
    NIMBLE_DBG_STAGE(3);
    os_msys_init();

#if NIMBLE_CFG_CONTROLLER
    /* Link layer (also brings up the PHY via SYSINIT-equivalent inside). */
    NIMBLE_DBG_STAGE(4);
    ble_ll_init();
#endif

    /* Transport + host. */
    NIMBLE_DBG_STAGE(5);
    ble_transport_init();
    NIMBLE_DBG_STAGE(6);
    ble_transport_hs_init();

#if NIMBLE_CFG_CONTROLLER
    /* Timer backend the LL scheduler + os_cputime ride on. hal_timer "5" is
     * the cputimer instance; os_cputime runs it at 32768 Hz. */
    NIMBLE_DBG_STAGE(7);
    hal_timer_init(5, NULL);
    NIMBLE_DBG_STAGE(8);
    os_cputime_init(32768);
#endif

    /* Controller transport (tells the host the controller is ready). */
    NIMBLE_DBG_STAGE(9);
    ble_transport_ll_init();

    NIMBLE_DBG_STAGE(10);
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
