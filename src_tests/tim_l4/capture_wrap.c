#include "model.h"
#include "../arm/semihost_test.h"

static void capture(uint32_t ccr, uint32_t count, uint32_t flags) {
    model_tim1.CCR1 = ccr;
    model_tim1.CNT = count;
    model_tim1.SR = TIM_SR_CC1IF | flags;
    TIM1_CC_IRQHandler();
}

static void update(void) {
    model_tim1.SR = TIM_SR_UIF;
    TIM1_UP_TIM16_IRQHandler();
}

static unsigned check(void) {
    static uni_hal_tim_channel_t input = {.type = UNI_HAL_TIM_TYPE_INPUTCAPTURE};
    static uni_hal_tim_channel_t *channels[] = {&input};
    static uni_hal_tim_context_t ctx = {.config = {
        .instance = UNI_HAL_CORE_PERIPH_TIM_1, .reload_value = 99U, .channel = channels, .channel_count = 1U,
    }};
    uni_hal_tim_channel_state_t sample;
    CHECK(uni_hal_tim_init(&ctx) && uni_hal_tim_start(&ctx));
    capture(40U, 45U, 0U);

    // The edge is 50 ticks after wrap; CC runs before the pending update ISR.
    capture(50U, 56U, TIM_SR_UIF);
    CHECK(uni_hal_tim_get_chan_state(&ctx, UNI_HAL_TIM_CHANNEL_1, &sample) && sample.period == 110U);
    update();

    // The next edge is before wrap; this time the update ISR runs first.
    update();
    capture(65U, 55U, 0U);
    CHECK(uni_hal_tim_get_chan_state(&ctx, UNI_HAL_TIM_CHANNEL_1, &sample) && sample.period == 15U);

    // Hardware wraps between reading CNT and checking UIF.
    wrap_on_counter_read = true;
    capture(80U, 99U, 0U);
    CHECK(uni_hal_tim_get_chan_state(&ctx, UNI_HAL_TIM_CHANNEL_1, &sample) && sample.period == 115U);
    update();
    capture(0U, 3U, 0U);
    CHECK(uni_hal_tim_get_chan_state(&ctx, UNI_HAL_TIM_CHANNEL_1, &sample) && sample.period == 20U);
    return 0;
}
