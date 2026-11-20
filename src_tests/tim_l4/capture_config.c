#include "model.h"
#include "../arm/semihost_test.h"

static unsigned check(void) {
    uni_hal_tim_channel_t input = {.type = UNI_HAL_TIM_TYPE_INPUTCAPTURE, .channel_number = UNI_HAL_TIM_CHANNEL_1};
    uni_hal_tim_channel_t output = {.type = UNI_HAL_TIM_TYPE_OUTPUTCOMPARE, .channel_number = UNI_HAL_TIM_CHANNEL_2};
    uni_hal_tim_channel_t *channels[] = {&input, &output};
    uni_hal_tim_context_t ctx = {.config = {
        .instance = UNI_HAL_CORE_PERIPH_TIM_16, .reload_value = 99U, .channel = channels, .channel_count = 1U,
    }};

    // TIM16 has a physical CH1 but the HAL vector only handles updates.
    CHECK(!uni_hal_tim_init(&ctx));
    CHECK(!uni_hal_tim_is_inited(&ctx) && !uni_hal_tim_start(&ctx) && clock_calls == 0U);
    ctx.config.channel_count = 0U;
    CHECK(uni_hal_tim_init(&ctx) && uni_hal_tim_start(&ctx) && uni_hal_tim_stop(&ctx));

    // Validate the whole list before configuring even its first, valid channel.
    ctx.config.instance = UNI_HAL_CORE_PERIPH_TIM_1;
    ctx.config.channel_count = 2U;
    channels[0] = &output;
    channels[1] = &input;
    input.channel_number = UNI_HAL_TIM_CHANNEL_5;
    CHECK(!uni_hal_tim_init(&ctx) && !uni_hal_tim_is_inited(&ctx) && clock_calls == 1U);
    channels[1] = nullptr;
    CHECK(!uni_hal_tim_init(&ctx) && clock_calls == 1U);
    ctx.config.channel = nullptr;
    CHECK(!uni_hal_tim_init(&ctx) && clock_calls == 1U);

    ctx.config.channel = channels;
    channels[1] = &input;
    input.channel_number = UNI_HAL_TIM_CHANNEL_4;
    CHECK(uni_hal_tim_init(&ctx) && clock_calls == 2U);
    CHECK(!LL_TIM_CC_IsEnabledChannel(&model_tim1, LL_TIM_CHANNEL_CH2));
    CHECK(uni_hal_tim_start(&ctx));
    CHECK(LL_TIM_CC_IsEnabledChannel(&model_tim1, LL_TIM_CHANNEL_CH2));
    CHECK(LL_TIM_IsEnabledAllOutputs(&model_tim1));
    CHECK(LL_TIM_IsEnabledIT_CC4(&model_tim1) && !LL_TIM_IsEnabledIT_CC2(&model_tim1));
    CHECK(uni_hal_tim_stop(&ctx));
    CHECK(!LL_TIM_IsEnabledAllOutputs(&model_tim1));
    CHECK(!LL_TIM_CC_IsEnabledChannel(&model_tim1, LL_TIM_CHANNEL_CH2));
    CHECK(uni_hal_tim_start(&ctx));
    CHECK(LL_TIM_CC_IsEnabledChannel(&model_tim1, LL_TIM_CHANNEL_CH2));
    CHECK(LL_TIM_IsEnabledAllOutputs(&model_tim1));
    CHECK(uni_hal_tim_stop(&ctx));

    // TIM2 has no break/dead-time register. Its CH1 works without setting MOE.
    output.channel_number = UNI_HAL_TIM_CHANNEL_1;
    ctx.config.instance = UNI_HAL_CORE_PERIPH_TIM_2;
    ctx.config.channel_count = 1U;
    CHECK(uni_hal_tim_init(&ctx) && uni_hal_tim_start(&ctx));
    CHECK(LL_TIM_CC_IsEnabledChannel(&model_tim2, LL_TIM_CHANNEL_CH1));
    CHECK(model_tim2.BDTR == 0U && !LL_TIM_IsEnabledIT_CC1(&model_tim2));
    CHECK(uni_hal_tim_stop(&ctx) && !LL_TIM_CC_IsEnabledChannel(&model_tim2, LL_TIM_CHANNEL_CH1));

    // TIM16 only exposes CH1.
    ctx.config.instance = UNI_HAL_CORE_PERIPH_TIM_16;
    output.channel_number = UNI_HAL_TIM_CHANNEL_2;
    CHECK(!uni_hal_tim_init(&ctx));
    return 0;
}
