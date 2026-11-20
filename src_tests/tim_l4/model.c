#include "model.h"

TIM_TypeDef model_tim1;
static TIM_TypeDef model_tim16;
#undef TIM1
#define TIM1 (&model_tim1)
#undef TIM16
#define TIM16 (&model_tim16)

bool wrap_on_counter_read;
static uint32_t sample_counter(const TIM_TypeDef *timer) {
    uint32_t const count = LL_TIM_GetCounter(timer);
    if (wrap_on_counter_read) {
        wrap_on_counter_read = false;
        model_tim1.CNT = 0;
        model_tim1.SR |= TIM_SR_UIF;
    }
    return count;
}

static bool model_clock(uni_hal_core_periph_e periph, bool enabled) {
    return periph == UNI_HAL_CORE_PERIPH_TIM_1 && enabled;
}

#define LL_TIM_GetCounter sample_counter
#define uni_hal_rcc_clk_set model_clock
#include "tim/uni_hal_tim_stm32l4.c"
