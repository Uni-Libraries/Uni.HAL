#pragma once

#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include <stm32h7xx.h>
#else
#include <stm32l4xx.h>
#endif

static RCC_TypeDef rcc_regs;
static RNG_TypeDef rng_regs = {.DR = 0x6429DC01U};
#undef RCC
#undef RNG
#define RCC (&rcc_regs)
#define RNG (&rng_regs)

#define uni_hal_rcc_clk_set model_clock_gate
#define uni_hal_rcc_clksrc_set model_clock_select
#define uni_hal_systick_get_ms model_tick

#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include "rng/uni_hal_rng_stm32h7.c"
#else
#include "rng/uni_hal_rng_stm32l4.c"
#endif
#include "../arm/semihost_test.h"

static unsigned polls, clock_ready_at, word_ready_at;
static uint32_t ms, step_ms;

uint32_t model_tick(void) {
    ++polls;
    if (polls == clock_ready_at) {
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
        rcc_regs.CR |= RCC_CR_HSI48RDY;
#else
        rcc_regs.CRRCR |= RCC_CRRCR_HSI48RDY;
#endif
    }
    if (polls == word_ready_at) {
        rng_regs.SR |= RNG_SR_DRDY;
    }
    ms += step_ms;
    return ms;
}

bool model_clock_gate(uni_hal_core_periph_e target, bool state) {
    return target == UNI_HAL_CORE_PERIPH_RNG && state;
}

#if defined(UNI_HAL_TARGET_MCU_STM32H743)
bool model_clock_select(uni_hal_core_periph_e target, uni_hal_rcc_clksrc_e source) {
    return target == UNI_HAL_CORE_PERIPH_RNG &&
           (source == UNI_HAL_RCC_CLKSRC_HSI48 || source == UNI_HAL_RCC_CLKSRC_PLL1Q);
}
#endif
