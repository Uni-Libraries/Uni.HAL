// Use ST's timer initialization and LL accessors on a register bank in RAM.
#include "model.h"

TIM_TypeDef model_tim15;
#undef TIM15
#define TIM15 (&model_tim15)

unsigned clock_calls;
static bool model_clock(uni_hal_core_periph_e instance, bool enable) {
    ++clock_calls;
    return instance == UNI_HAL_CORE_PERIPH_TIM_15 && enable;
}
#define uni_hal_rcc_clk_set model_clock
#include "tim/uni_hal_tim_stm32h7.c"
