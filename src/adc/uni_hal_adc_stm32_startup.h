#pragma once

#include "systick/uni_hal_systick_stm32_wait.h"

// Each phase gets a fresh timeout. The default allows H743 linearity calibration
// to take about 165000 ADC clock cycles.
static inline bool uni_hal_adc_wait_startup_flag(const volatile uint32_t *reg, uint32_t flag,
                                                bool set, uint32_t timeout_ms) {
    return uni_hal_stm32_wait_bits(reg, flag, set ? flag : 0U, timeout_ms ? timeout_ms : 500U);
}
