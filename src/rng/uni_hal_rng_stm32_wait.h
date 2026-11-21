#pragma once

#if defined(STM32H743xx) || defined(STM32H753xx)
#include <stm32h7xx_ll_rcc.h>
#include <stm32h7xx_ll_rng.h>
#else
#include <stm32l4xx_ll_rcc.h>
#include <stm32l4xx_ll_rng.h>
#endif
#include "systick/uni_hal_systick_stm32_wait.h"

enum { UNI_HAL_RNG_TIMEOUT_MS = 100U };

static inline bool uni_hal_rng_stm32_start_hsi48(void) {
    LL_RCC_HSI48_Enable();
    uni_hal_stm32_poll_budget_t budget = uni_hal_stm32_poll_begin(UNI_HAL_RNG_TIMEOUT_MS);
    while (LL_RCC_HSI48_IsReady() == 0U) {
        if (uni_hal_stm32_poll_expired(&budget)) {
            return false;
        }
    }
    return true;
}

static inline bool uni_hal_rng_stm32_read_word(RNG_TypeDef *instance, uint32_t *out) {
    uni_hal_stm32_poll_budget_t budget = uni_hal_stm32_poll_begin(UNI_HAL_RNG_TIMEOUT_MS);
    for (;;) {
        if (LL_RNG_IsActiveFlag_SEIS(instance) != 0U) {
            // Discard the word on a seed error and restart within the same timeout.
            LL_RNG_ClearFlag_SEIS(instance);
            LL_RNG_Disable(instance);
            LL_RNG_Enable(instance);
        } else if (LL_RNG_IsActiveFlag_DRDY(instance) != 0U) {
            *out = LL_RNG_ReadRandData32(instance);
            return true;
        }

        if (LL_RNG_IsActiveFlag_CEIS(instance) != 0U) {
            LL_RNG_ClearFlag_CEIS(instance);
        }
        if (uni_hal_stm32_poll_expired(&budget)) {
            return false;
        }
    }
}
