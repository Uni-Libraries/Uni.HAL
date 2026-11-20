#pragma once

#include <stm32l4xx_ll_tim.h>
#include "tim/uni_hal_tim.h"

extern TIM_TypeDef model_tim1, model_tim2;
extern bool wrap_on_counter_read;
extern unsigned clock_calls;
void TIM1_CC_IRQHandler(void);
void TIM1_UP_TIM16_IRQHandler(void);
