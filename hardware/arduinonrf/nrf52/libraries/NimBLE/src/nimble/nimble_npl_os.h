// nimble_npl_os.h - bare-metal NimBLE Porting Layer for the nRF52840
// Arduino core. Replaces the Apache Mynewt dummy port with structures
// that actually carry state, sized for cooperative scheduling out of a
// single main loop.
//
// Design constraints:
//   * Single-threaded - no preemption. Mutex/Sem are flag-based; they never
//     block in this port (the caller is responsible for not deadlocking).
//   * Critical sections use Cortex-M PRIMASK (save/restore via __disable_irq).
//   * Event queues are intrusive singly-linked lists guarded by critical
//     sections.
//   * Callouts use the existing NrfRtc / NrfTimer driver underneath
//     (the wiring lands in M2; current M1 has timer hooks stubbed).
//
// Public surface (called from libraries/NimBLE/src/NimBLE.cpp + from the
// NimBLE host / controller code once vendored) is declared in
// vendor/include/nimble/nimble_npl.h. This header just defines the
// platform-specific struct layouts.

#ifndef NIMBLE_NPL_BARE_OS_H_
#define NIMBLE_NPL_BARE_OS_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_NPL_OS_ALIGNMENT    4
#define BLE_NPL_TIME_FOREVER    UINT32_MAX

typedef uint32_t ble_npl_time_t;
typedef int32_t  ble_npl_stime_t;

struct ble_npl_event;
typedef void ble_npl_event_fn(struct ble_npl_event *ev);

// Intrusive event - the list pointer + queued flag are part of the event
// itself so we don't need dynamic allocation.
struct ble_npl_event {
    bool                  queued;
    struct ble_npl_event *next;
    ble_npl_event_fn     *fn;
    void                 *arg;
};

struct ble_npl_eventq {
    struct ble_npl_event *head;
    struct ble_npl_event *tail;
};

// Callout - a one-shot timer that posts an event when it expires.
struct ble_npl_callout {
    struct ble_npl_event    ev;
    struct ble_npl_eventq  *evq;
    uint32_t                expiration_ticks;
    bool                    active;
    struct ble_npl_callout *next;     // intrusive list of pending callouts
};

// Mutex / Sem - cooperative scheduling makes these flag-based.
struct ble_npl_mutex { volatile uint32_t locked; };
struct ble_npl_sem   { volatile uint32_t count;  };

#ifdef __cplusplus
}
#endif

#endif  // NIMBLE_NPL_BARE_OS_H_
