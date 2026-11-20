#pragma once

#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include <stm32h7xx.h>
#else
#include <stm32l4xx.h>
#endif

// Run both production drivers against RAM registers, including the single RCC ADCSEL field.
static RCC_TypeDef rcc;
#undef RCC
#define RCC (&rcc)
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include "rcc/uni_hal_rcc_stm32h7.c"
#else
#include "rcc/uni_hal_rcc_stm32l4.c"
#endif
