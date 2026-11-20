#pragma once

#include <stm32l4xx.h>

static PWR_TypeDef pwr;
#undef PWR
#define PWR (&pwr)
#define uni_hal_rcc_clk_set model_clock_set
#define uni_hal_systick_get_ms model_tick
#include "pwr/uni_hal_pwr_stm32l4.c"
#include "../arm/semihost_test.h"

static unsigned polls, ready_at;
static uint32_t ms, step_ms;

bool model_clock_set(uni_hal_core_periph_e periph, bool enable) {
    return enable && (periph == UNI_HAL_CORE_PERIPH_SYSCFG || periph == UNI_HAL_CORE_PERIPH_PWR);
}

uint32_t model_tick(void) {
    if (++polls == ready_at) {
        pwr.SR2 &= ~PWR_SR2_VOSF;
    }
    ms += step_ms;
    return ms;
}
