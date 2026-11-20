#include "model.h"
#include "../arm/semihost_test.h"

static unsigned check(void) {
    static const struct {
        uni_hal_core_periph_e instance;
        TIM_TypeDef *registers;
        uint32_t max_reload;
    } timers[] = {
        {UNI_HAL_CORE_PERIPH_TIM_2, &model_tim2, UINT32_MAX},
        {UNI_HAL_CORE_PERIPH_TIM_15, &model_tim15, UINT16_MAX},
        {UNI_HAL_CORE_PERIPH_TIM_16, &model_tim16, UINT16_MAX},
        {UNI_HAL_CORE_PERIPH_TIM_17, &model_tim17, UINT16_MAX},
    };
    for (size_t i = 0; i < sizeof timers / sizeof timers[0]; ++i) {
        uni_hal_tim_context_t ctx = {.config = {
            .instance = timers[i].instance,
            .prescaler = UINT16_MAX,
            .reload_value = timers[i].max_reload,
        }};
        CHECK(uni_hal_tim_init(&ctx));
        const unsigned clocks = clock_calls;
        ctx.config.prescaler = UINT16_MAX + 1U;
        CHECK(!uni_hal_tim_init(&ctx) && !uni_hal_tim_is_inited(&ctx));
        if (timers[i].max_reload == UINT16_MAX) {
            ctx.config.prescaler = 0U;
            ctx.config.reload_value = UINT16_MAX + 1U;
            CHECK(!uni_hal_tim_init(&ctx) && !uni_hal_tim_is_inited(&ctx));
        }
        CHECK(clock_calls == clocks);
        CHECK(timers[i].registers->PSC == UINT16_MAX);
        CHECK(timers[i].registers->ARR == timers[i].max_reload);
    }
    return 0;
}
