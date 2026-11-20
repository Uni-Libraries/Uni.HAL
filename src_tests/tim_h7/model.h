#pragma once

#include <stm32h7xx_ll_tim.h>
#include "tim/uni_hal_tim.h"

extern TIM_TypeDef model_tim2, model_tim15, model_tim16, model_tim17;
extern bool clock_failed;
extern unsigned clock_calls;
