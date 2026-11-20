#pragma once

#include "systick/uni_hal_systick.h"

// Each phase gets a fresh timeout. The default allows H743 linearity calibration
// to take about 165000 ADC clock cycles.
static inline bool uni_hal_adc_wait_startup_flag(const volatile uint32_t *reg, uint32_t flag,
                                                bool set, uint32_t timeout_ms) {
    uint32_t remaining = timeout_ms ? timeout_ms : 500U;
    uint32_t previous = uni_hal_systick_get_ms();
    while (((*reg & flag) != 0U) != set) {
        if (remaining == 0U) {
            return false;
        }
        uint32_t const now = uni_hal_systick_get_ms();
        // A coarse tick can skip the deadline and wrap the total elapsed time.
        uint32_t const elapsed = now - previous;
        if (elapsed > remaining) {
            return false;
        }
        remaining -= elapsed;
        previous = now;
    }
    return true;
}
