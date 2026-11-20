#pragma once
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include <stm32h7xx_ll_iwdg.h>
#else
#include <stm32l4xx_ll_iwdg.h>
#endif

static IWDG_TypeDef watchdog;
static unsigned reloads;
static void model_reload(IWDG_TypeDef *target) {
    ++reloads;
    if (target == &watchdog) {
        LL_IWDG_ReloadCounter(target);
    }
}

#define LL_IWDG_ReloadCounter model_reload
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#undef IWDG1
#define IWDG1 (&watchdog)
#include "iwdg/uni_hal_iwdg_stm32h7.c"
#else
#undef IWDG
#define IWDG (&watchdog)
#include "iwdg/uni_hal_iwdg_stm32l4.c"
#endif
