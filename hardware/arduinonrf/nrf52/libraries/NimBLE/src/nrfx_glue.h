// Minimal nrfx glue for the ArduinoNRF NimBLE controller probe.
#ifndef NRFX_GLUE_H__
#define NRFX_GLUE_H__

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "nrf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NRFX_ASSERT(expression) assert(expression)
#define NRFX_STATIC_ASSERT(expression) _Static_assert((expression), "nrfx static assert")

#define NRFX_IRQ_PRIORITY_SET(irq_number, priority) NVIC_SetPriority((IRQn_Type)(irq_number), (priority))
#define NRFX_IRQ_ENABLE(irq_number) NVIC_EnableIRQ((IRQn_Type)(irq_number))
#define NRFX_IRQ_IS_ENABLED(irq_number) (NVIC_GetEnableIRQ((IRQn_Type)(irq_number)) != 0)
#define NRFX_IRQ_DISABLE(irq_number) NVIC_DisableIRQ((IRQn_Type)(irq_number))
#define NRFX_IRQ_PENDING_SET(irq_number) NVIC_SetPendingIRQ((IRQn_Type)(irq_number))
#define NRFX_IRQ_PENDING_CLEAR(irq_number) NVIC_ClearPendingIRQ((IRQn_Type)(irq_number))
#define NRFX_IRQ_IS_PENDING(irq_number) (NVIC_GetPendingIRQ((IRQn_Type)(irq_number)) != 0)

#define NRFX_CRITICAL_SECTION_ENTER() uint32_t nrfx_critical_section_state = __get_PRIMASK(); __disable_irq()
#define NRFX_CRITICAL_SECTION_EXIT() do { if (nrfx_critical_section_state == 0U) { __enable_irq(); } } while (0)

#define NRFX_COREDEP_DELAY_DWT_BASED 0
#define NRFX_DELAY_US(us_time) do { for (volatile uint32_t nrfx_delay_spin = 0; nrfx_delay_spin < ((uint32_t)(us_time) * 16U); ++nrfx_delay_spin) { __NOP(); } } while (0)

typedef uint32_t nrfx_atomic_t;

static inline uint32_t nrfx_atomic_fetch_store_u32(volatile uint32_t *ptr, uint32_t value) {
    uint32_t old_value = *ptr;
    *ptr = value;
    return old_value;
}

static inline uint32_t nrfx_atomic_fetch_or_u32(volatile uint32_t *ptr, uint32_t value) {
    uint32_t old_value = *ptr;
    *ptr |= value;
    return old_value;
}

static inline uint32_t nrfx_atomic_fetch_and_u32(volatile uint32_t *ptr, uint32_t value) {
    uint32_t old_value = *ptr;
    *ptr &= value;
    return old_value;
}

static inline uint32_t nrfx_atomic_fetch_xor_u32(volatile uint32_t *ptr, uint32_t value) {
    uint32_t old_value = *ptr;
    *ptr ^= value;
    return old_value;
}

static inline uint32_t nrfx_atomic_fetch_add_u32(volatile uint32_t *ptr, uint32_t value) {
    uint32_t old_value = *ptr;
    *ptr += value;
    return old_value;
}

static inline uint32_t nrfx_atomic_fetch_sub_u32(volatile uint32_t *ptr, uint32_t value) {
    uint32_t old_value = *ptr;
    *ptr -= value;
    return old_value;
}

#define NRFX_ATOMIC_FETCH_STORE(p_data, value) nrfx_atomic_fetch_store_u32((volatile uint32_t *)(p_data), (uint32_t)(value))
#define NRFX_ATOMIC_FETCH_OR(p_data, value) nrfx_atomic_fetch_or_u32((volatile uint32_t *)(p_data), (uint32_t)(value))
#define NRFX_ATOMIC_FETCH_AND(p_data, value) nrfx_atomic_fetch_and_u32((volatile uint32_t *)(p_data), (uint32_t)(value))
#define NRFX_ATOMIC_FETCH_XOR(p_data, value) nrfx_atomic_fetch_xor_u32((volatile uint32_t *)(p_data), (uint32_t)(value))
#define NRFX_ATOMIC_FETCH_ADD(p_data, value) nrfx_atomic_fetch_add_u32((volatile uint32_t *)(p_data), (uint32_t)(value))
#define NRFX_ATOMIC_FETCH_SUB(p_data, value) nrfx_atomic_fetch_sub_u32((volatile uint32_t *)(p_data), (uint32_t)(value))

#define NRFX_CLZ(value) __CLZ(value)
#define NRFX_CTZ(value) __CLZ(__RBIT(value))

#ifdef __cplusplus
}
#endif

#endif