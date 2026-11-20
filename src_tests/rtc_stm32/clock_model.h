#pragma once

#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include <stm32h7xx.h>
#else
#include <stm32l4xx.h>
#endif

static RCC_TypeDef rcc;
static PWR_TypeDef pwr;
#undef RCC
#undef PWR
#define RCC (&rcc)
#define PWR (&pwr)

#include "pwr/uni_hal_pwr_stm32.c"
