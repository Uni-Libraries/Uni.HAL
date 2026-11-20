#include "model.h"
#include "../arm/semihost_test.h"

static unsigned check(void) {
    uni_hal_tim_channel_t channel = {.channel_number = UNI_HAL_TIM_CHANNEL_1};
    uni_hal_tim_channel_t *channels[] = {&channel};
    uni_hal_tim_context_t ctx = {.config = {
        .instance = UNI_HAL_CORE_PERIPH_TIM_15, .prescaler = 7U, .reload_value = 199U,
        .channel = channels, .channel_count = 1U}};

    channel.type = UNI_HAL_TIM_TYPE_INPUTCAPTURE;
    CHECK(!uni_hal_tim_init(&ctx) && !uni_hal_tim_is_inited(&ctx));
    channel.type = UNI_HAL_TIM_TYPE_OUTPUTCOMPARE;
    CHECK(!uni_hal_tim_init(&ctx) && !uni_hal_tim_is_inited(&ctx));
    ctx.config.channel = nullptr;
    CHECK(!uni_hal_tim_init(&ctx));
    CHECK(clock_calls == 0U && model_tim15.EGR == 0U && model_tim15.ARR == 0U);
    CHECK(model_tim15.CCMR1 == 0U && model_tim15.CCER == 0U);

    ctx.config.channel_count = 0U;
    CHECK(uni_hal_tim_init(&ctx) && uni_hal_tim_is_inited(&ctx));
    CHECK(clock_calls == 1U && model_tim15.PSC == 7U && model_tim15.ARR == 199U);
    return 0;
}
