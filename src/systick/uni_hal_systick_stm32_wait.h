#pragma once

#include "systick/uni_hal_systick.h"
#if defined(STM32H743xx)
#include <stm32h7xx.h>
#else
#include <stm32l4xx.h>
#endif

typedef struct {
    uint32_t previous_ms;
    uint32_t remaining_ms;
    uint64_t polls_left;
} uni_hal_stm32_poll_budget_t;

static inline uni_hal_stm32_poll_budget_t uni_hal_stm32_poll_begin(uint32_t timeout_ms) {
    // At most one poll can complete per core cycle. This conservative bound also
    // works during early startup or with SysTick masked, without reserving DWT.
    return (uni_hal_stm32_poll_budget_t){
        .previous_ms = uni_hal_systick_get_ms(),
        .remaining_ms = timeout_ms,
        .polls_left = (uint64_t)(SystemCoreClock / 1000U + 1U) * timeout_ms,
    };
}

static inline bool uni_hal_stm32_poll_expired(uni_hal_stm32_poll_budget_t *budget) {
    if (budget->remaining_ms == 0U || budget->polls_left == 0U) {
        return true;
    }
    --budget->polls_left;
    uint32_t const now = uni_hal_systick_get_ms();
    uint32_t const elapsed = now - budget->previous_ms;
    if (elapsed > budget->remaining_ms) {
        return true;
    }
    budget->remaining_ms -= elapsed;
    budget->previous_ms = now;
    return false;
}

static inline bool uni_hal_stm32_wait_bits(const volatile uint32_t *reg, uint32_t mask,
                                          uint32_t expected, uint32_t timeout_ms) {
    uni_hal_stm32_poll_budget_t budget = uni_hal_stm32_poll_begin(timeout_ms);
    while ((*reg & mask) != expected) {
        if (uni_hal_stm32_poll_expired(&budget)) {
            return false;
        }
    }
    return true;
}
