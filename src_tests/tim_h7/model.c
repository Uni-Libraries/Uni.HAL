// Use ST's timer initialization and LL accessors on a register bank in RAM.
#include "model.h"

TIM_TypeDef model_tim2, model_tim15, model_tim16, model_tim17;
#undef TIM2
#define TIM2 (&model_tim2)
#undef TIM15
#define TIM15 (&model_tim15)
#undef TIM16
#define TIM16 (&model_tim16)
#undef TIM17
#define TIM17 (&model_tim17)

unsigned clock_calls;
bool clock_failed;
static bool model_clock(uni_hal_core_periph_e instance, bool enable) {
    ++clock_calls;
    (void)instance;
    return enable && !clock_failed;
}
#define uni_hal_rcc_clk_set model_clock
#include "tim/uni_hal_tim_stm32h7.c"
