// npl_os_bare.c - bare-metal Apache NimBLE Porting Layer implementation
// for the nRF52840 Arduino core.
//
// Implements every ble_npl_* function declared in
// vendor/include/nimble/nimble_npl.h with Cortex-M primitives:
//   * critical sections via PRIMASK save/restore
//   * event queues as intrusive singly-linked lists
//   * timing from RTC1 (32.768 kHz LFCLK)
//   * mutex/sem are flag-only since we have no preemption
//
// M1 status (this file):
//   * Critical section, event queue, eventq init / put / get / remove
//     are FULLY FUNCTIONAL.
//   * Callout / time delay / time-to-ticks are partially functional;
//     they use the OS_CPUTIME hook into NimBLE's own RTC abstraction once
//     M2 hooks the RTC1 backend.
//   * Mutex / sem are flag stubs that never block (correct for the
//     single-threaded design - the NimBLE host is run from the main
//     event loop, not preempted).

#include <stddef.h>
#include <string.h>
#include "nimble/nimble_npl.h"

// Defined in nimble_port_bare.c - drains the host + LL event queues once,
// non-blocking. ble_npl_sem_pend() pumps it to cooperatively wait for events.
void nimble_port_run(void);

// ---- Critical sections via PRIMASK -----------------------------------------

uint32_t ble_npl_hw_enter_critical(void) {
    uint32_t primask;
    __asm volatile ("mrs %0, primask\n cpsid i" : "=r"(primask));
    return primask;
}

void ble_npl_hw_exit_critical(uint32_t ctx) {
    __asm volatile ("msr primask, %0" : : "r"(ctx) : "memory");
}

bool ble_npl_hw_is_in_critical(void) {
    uint32_t primask;
    __asm volatile ("mrs %0, primask" : "=r"(primask));
    return primask != 0U;
}

// ---- Event queue (intrusive singly-linked list) ----------------------------

void ble_npl_eventq_init(struct ble_npl_eventq *evq) {
    evq->head = NULL;
    evq->tail = NULL;
}

void ble_npl_eventq_deinit(struct ble_npl_eventq *evq) {
    evq->head = NULL;
    evq->tail = NULL;
}

void ble_npl_eventq_put(struct ble_npl_eventq *evq, struct ble_npl_event *ev) {
    uint32_t ctx = ble_npl_hw_enter_critical();
    if (ev->queued) {
        ble_npl_hw_exit_critical(ctx);
        return;
    }
    ev->queued = true;
    ev->next = NULL;
    if (evq->tail) {
        evq->tail->next = ev;
        evq->tail = ev;
    } else {
        evq->head = ev;
        evq->tail = ev;
    }
    ble_npl_hw_exit_critical(ctx);
}

struct ble_npl_event *ble_npl_eventq_get(struct ble_npl_eventq *evq,
                                          ble_npl_time_t tmo) {
    (void)tmo;   // we don't block; this port is poll-only
    uint32_t ctx = ble_npl_hw_enter_critical();
    struct ble_npl_event *ev = evq->head;
    if (ev) {
        evq->head = ev->next;
        if (evq->head == NULL) {
            evq->tail = NULL;
        }
        ev->queued = false;
        ev->next = NULL;
    }
    ble_npl_hw_exit_critical(ctx);
    return ev;
}

struct ble_npl_event *ble_npl_eventq_get_no_wait(struct ble_npl_eventq *evq) {
    return ble_npl_eventq_get(evq, 0U);
}

void ble_npl_eventq_remove(struct ble_npl_eventq *evq,
                            struct ble_npl_event *ev) {
    uint32_t ctx = ble_npl_hw_enter_critical();
    if (!ev->queued) {
        ble_npl_hw_exit_critical(ctx);
        return;
    }
    struct ble_npl_event *prev = NULL;
    struct ble_npl_event *cur = evq->head;
    while (cur) {
        if (cur == ev) {
            if (prev) prev->next = cur->next;
            else      evq->head  = cur->next;
            if (evq->tail == cur) evq->tail = prev;
            cur->next = NULL;
            cur->queued = false;
            break;
        }
        prev = cur;
        cur = cur->next;
    }
    ble_npl_hw_exit_critical(ctx);
}

bool ble_npl_eventq_is_empty(struct ble_npl_eventq *evq) {
    uint32_t ctx = ble_npl_hw_enter_critical();
    bool empty = (evq->head == NULL);
    ble_npl_hw_exit_critical(ctx);
    return empty;
}

// ---- Event ----------------------------------------------------------------

void ble_npl_event_init(struct ble_npl_event *ev, ble_npl_event_fn *fn,
                        void *arg) {
    ev->queued = false;
    ev->next = NULL;
    ev->fn = fn;
    ev->arg = arg;
}

void ble_npl_event_deinit(struct ble_npl_event *ev) {
    ev->fn = NULL;
    ev->arg = NULL;
}

void ble_npl_event_reset(struct ble_npl_event *ev) {
    ev->queued = false;
    ev->next = NULL;
}

bool ble_npl_event_is_queued(struct ble_npl_event *ev) { return ev->queued; }

void *ble_npl_event_get_arg(struct ble_npl_event *ev) { return ev->arg; }

void ble_npl_event_set_arg(struct ble_npl_event *ev, void *arg) {
    ev->arg = arg;
}

void ble_npl_event_run(struct ble_npl_event *ev) {
    if (ev->fn) ev->fn(ev);
}

// ---- Callout (timer-backed) -----------------------------------------------
//
// M1 stub: stores parameters but doesn't actually fire. M2 wires this to
// NrfTimer1 (the link-layer timer) - call ble_npl_callout_inserted_tick()
// from the timer ISR to expire pending callouts.

static struct ble_npl_callout *s_callout_list = NULL;

void ble_npl_callout_init(struct ble_npl_callout *co, struct ble_npl_eventq *evq,
                           ble_npl_event_fn *fn, void *arg) {
    memset(co, 0, sizeof(*co));
    co->evq = evq;
    ble_npl_event_init(&co->ev, fn, arg);
}

void ble_npl_callout_deinit(struct ble_npl_callout *co) {
    ble_npl_callout_stop(co);
    memset(co, 0, sizeof(*co));
}

ble_npl_error_t ble_npl_callout_reset(struct ble_npl_callout *co,
                                       ble_npl_time_t ticks) {
    uint32_t ctx = ble_npl_hw_enter_critical();
    co->active = true;
    co->expiration_ticks = ticks;   // M2 will translate to absolute RTC ticks
    // Insert at head of pending list (M2 will sort by expiration).
    co->next = s_callout_list;
    s_callout_list = co;
    ble_npl_hw_exit_critical(ctx);
    return BLE_NPL_OK;
}

void ble_npl_callout_stop(struct ble_npl_callout *co) {
    uint32_t ctx = ble_npl_hw_enter_critical();
    co->active = false;
    // Remove from pending list.
    struct ble_npl_callout *prev = NULL;
    struct ble_npl_callout *cur = s_callout_list;
    while (cur) {
        if (cur == co) {
            if (prev) prev->next = cur->next;
            else      s_callout_list = cur->next;
            cur->next = NULL;
            break;
        }
        prev = cur;
        cur = cur->next;
    }
    ble_npl_hw_exit_critical(ctx);
}

bool ble_npl_callout_is_active(struct ble_npl_callout *co) {
    return co->active;
}

ble_npl_time_t ble_npl_callout_get_ticks(struct ble_npl_callout *co) {
    return co->expiration_ticks;
}

ble_npl_time_t ble_npl_callout_remaining_ticks(struct ble_npl_callout *co,
                                                ble_npl_time_t now) {
    (void)now;
    return co->expiration_ticks;   // M2 makes this absolute
}

void ble_npl_callout_set_arg(struct ble_npl_callout *co, void *arg) {
    ble_npl_event_set_arg(&co->ev, arg);
}

// ---- Time / delay ---------------------------------------------------------

// Default tick rate when the NimBLE port is set up against RTC1. M2 will
// install the real RTC1 backend.
#define BLE_NPL_TICKS_PER_SECOND 32768U

ble_npl_time_t ble_npl_time_get(void) {
    // M1 placeholder - returns 0 until M2 hooks RTC1.
    return 0U;
}

bool ble_npl_os_started(void) {
    return false;
}

ble_npl_error_t ble_npl_time_ms_to_ticks(uint32_t ms, ble_npl_time_t *out_ticks) {
    *out_ticks = (uint32_t)(((uint64_t)ms * BLE_NPL_TICKS_PER_SECOND) / 1000U);
    return BLE_NPL_OK;
}

ble_npl_error_t ble_npl_time_ticks_to_ms(ble_npl_time_t ticks, uint32_t *out_ms) {
    *out_ms = (uint32_t)(((uint64_t)ticks * 1000U) / BLE_NPL_TICKS_PER_SECOND);
    return BLE_NPL_OK;
}

ble_npl_time_t ble_npl_time_ms_to_ticks32(uint32_t ms) {
    return (uint32_t)(((uint64_t)ms * BLE_NPL_TICKS_PER_SECOND) / 1000U);
}

uint32_t ble_npl_time_ticks_to_ms32(ble_npl_time_t ticks) {
    return (uint32_t)(((uint64_t)ticks * 1000U) / BLE_NPL_TICKS_PER_SECOND);
}

void ble_npl_time_delay(ble_npl_time_t ticks) {
    (void)ticks;   // M2: busy-wait via NrfTimer, or sleep via NrfPower::sleepMs
}

void *ble_npl_get_current_task_id(void) {
    return (void *)1;
}

// ---- Mutex / Sem (flag-only - single-threaded port) -----------------------

ble_npl_error_t ble_npl_mutex_init(struct ble_npl_mutex *mu) {
    mu->locked = 0;
    return BLE_NPL_OK;
}
ble_npl_error_t ble_npl_mutex_deinit(struct ble_npl_mutex *mu) {
    mu->locked = 0;
    return BLE_NPL_OK;
}
ble_npl_error_t ble_npl_mutex_pend(struct ble_npl_mutex *mu, ble_npl_time_t tmo) {
    (void)tmo;
    uint32_t ctx = ble_npl_hw_enter_critical();
    mu->locked = 1;
    ble_npl_hw_exit_critical(ctx);
    return BLE_NPL_OK;
}
ble_npl_error_t ble_npl_mutex_release(struct ble_npl_mutex *mu) {
    mu->locked = 0;
    return BLE_NPL_OK;
}

ble_npl_error_t ble_npl_sem_init(struct ble_npl_sem *sem, uint16_t tokens) {
    sem->count = tokens;
    return BLE_NPL_OK;
}
ble_npl_error_t ble_npl_sem_deinit(struct ble_npl_sem *sem) {
    sem->count = 0;
    return BLE_NPL_OK;
}
ble_npl_error_t ble_npl_sem_pend(struct ble_npl_sem *sem, ble_npl_time_t tmo) {
    // Fast path: a token is already available.
    uint32_t ctx = ble_npl_hw_enter_critical();
    if (sem->count > 0) {
        sem->count--;
        ble_npl_hw_exit_critical(ctx);
        return BLE_NPL_OK;
    }
    ble_npl_hw_exit_critical(ctx);

    // Cooperative wait: no RTOS, so block by PUMPING the event queues until the
    // awaited token is posted (e.g. the host's HCI command-complete event that
    // releases ble_hs_hci_sem). This is what makes synchronous host<->controller
    // HCI round-trips work on bare metal. Bounded so a missing response times
    // out instead of hanging. tmo==0 means a single non-blocking poll.
    uint32_t spins = (tmo == 0) ? 1u : 1000000u;
    while (spins--) {
        nimble_port_run();   // drains host + LL queues (non-blocking)
        ctx = ble_npl_hw_enter_critical();
        if (sem->count > 0) {
            sem->count--;
            ble_npl_hw_exit_critical(ctx);
            return BLE_NPL_OK;
        }
        ble_npl_hw_exit_critical(ctx);
    }
    return BLE_NPL_TIMEOUT;
}
ble_npl_error_t ble_npl_sem_release(struct ble_npl_sem *sem) {
    uint32_t ctx = ble_npl_hw_enter_critical();
    sem->count++;
    ble_npl_hw_exit_critical(ctx);
    return BLE_NPL_OK;
}
uint16_t ble_npl_sem_get_count(struct ble_npl_sem *sem) {
    return (uint16_t)sem->count;
}

// ---- HW IRQ hook ----------------------------------------------------------
//
// NimBLE controller wants to install ISRs for the radio + clock + timer
// peripherals it owns. In our setup the Arduino core's startup_nrf52.cpp
// defines those as weak aliases to Default_Handler; M2 will provide the
// real radio ISRs that forward into NimBLE's controller.
//
// For now this just stores the pointer so the controller can run the basic
// init sequence without faulting.

static void (*s_isr_table[64])(void) = {0};

void ble_npl_hw_set_isr(int irqn, void (*addr)(void)) {
    if (irqn >= 0 && irqn < (int)(sizeof(s_isr_table) / sizeof(s_isr_table[0]))) {
        s_isr_table[irqn] = addr;
    }
}

// Real peripheral IRQ vectors for the controller-owned peripherals, dispatched
// into the ISRs the controller registered via ble_npl_hw_set_isr(). The
// Arduino core declares these as weak Default_Handler aliases, so these strong
// definitions take over when the NimBLE library is linked into a sketch.
// (The LL timer backend installs its own TIMER/RTC handlers via hal_timer.)
void RADIO_IRQHandler(void) { if (s_isr_table[1])  s_isr_table[1](); }   // RADIO_IRQn = 1
void RNG_IRQHandler(void)   { if (s_isr_table[13]) s_isr_table[13](); }  // RNG_IRQn  = 13
