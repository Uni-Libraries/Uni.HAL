#pragma once

#include "dwt/uni_hal_dwt.h"
#include "systick/uni_hal_systick_stm32_wait.h"

typedef struct {
    uni_hal_stm32_poll_budget_t polling;
    uint32_t previous_cycle;
    uint64_t cycles_left;
} uni_hal_stm32_clock_timeout_t;

static inline uni_hal_stm32_clock_timeout_t uni_hal_stm32_clock_timeout_begin(uint32_t timeout_ms) {
    uni_hal_stm32_clock_timeout_t timeout = {.polling = uni_hal_stm32_poll_begin(timeout_ms)};
    if (SystemCoreClock != 0U && uni_hal_dwt_is_inited()) {
        timeout.previous_cycle = uni_hal_dwt_get_tick();
        timeout.cycles_left = ((uint64_t)timeout_ms * SystemCoreClock + 999U) / 1000U;
    }
    return timeout;
}

static inline bool uni_hal_stm32_clock_timeout_expired(uni_hal_stm32_clock_timeout_t *timeout) {
    if (uni_hal_stm32_poll_expired(&timeout->polling)) {
        return true;
    }
    if (timeout->cycles_left != 0U) {
        // CYCCNT measures elapsed time while SysTick is masked. Long timeouts can span several wraps.
        uint32_t const cycle = uni_hal_dwt_get_tick();
        uint32_t const elapsed = uni_hal_dwt_compare(timeout->previous_cycle, cycle);
        if (elapsed >= timeout->cycles_left) {
            return true;
        }
        timeout->cycles_left -= elapsed;
        timeout->previous_cycle = cycle;
    }
    return false;
}
