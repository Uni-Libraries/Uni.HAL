#include "model.h"
#include "../arm/semihost_test.h"

static void compare_events(unsigned input) {
    model_tim1.CNT = 10U;
    model_tim1.CCR1 = input == 0U ? 10U : 0U;
    model_tim1.CCR2 = input == 1U ? 10U : 0U;
    model_tim1.CCR3 = input == 2U ? 10U : 0U;
    model_tim1.CCR4 = input == 3U ? 10U : 0U;
    // Output compares matched at zero. Only the input channel requested an IRQ.
    model_tim1.SR = TIM_SR_CC1IF | TIM_SR_CC2IF | TIM_SR_CC3IF | TIM_SR_CC4IF;
    TIM1_CC_IRQHandler();
}

static unsigned check(void) {
    static uni_hal_tim_channel_t config[4];
    static uni_hal_tim_channel_t *channels[] = {config, config + 1, config + 2, config + 3};
    static uni_hal_tim_context_t ctx = {.config = {
        .instance = UNI_HAL_CORE_PERIPH_TIM_1, .reload_value = 99U, .channel = channels, .channel_count = 4U,
    }};
    for (unsigned input = 0U; input < 4U; ++input) {
        for (unsigned i = 0U; i < 4U; ++i) {
            config[i].channel_number = (uni_hal_tim_channel_num_e)i;
            config[i].type = i == input ? UNI_HAL_TIM_TYPE_INPUTCAPTURE : UNI_HAL_TIM_TYPE_OUTPUTCOMPARE;
        }
        CHECK(uni_hal_tim_init(&ctx) && uni_hal_tim_start(&ctx));
        compare_events(input);
        model_tim1.SR = TIM_SR_UIF;
        TIM1_UP_TIM16_IRQHandler();
        compare_events(input);

        uni_hal_tim_channel_state_t sample;
        CHECK(uni_hal_tim_get_chan_state(&ctx, config[input].channel_number, &sample) && sample.period == 100U);
        for (unsigned i = 0U; i < 4U; ++i) {
            if (i != input) {
                CHECK(!uni_hal_tim_get_chan_state(&ctx, config[i].channel_number, &sample));
            }
        }
        CHECK(uni_hal_tim_stop(&ctx));
    }
    return 0;
}
