#include "model.h"

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (false)

unsigned test_adc(void) {
    static const struct {
        uint32_t input;
        uint32_t sequencer_channel;
        bool routed[3]; // ADC1, ADC2, ADC3
    } routes[] = {
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
        {UNI_HAL_ADC_STM32H7_CHANNEL_REFINT,     19U, {false, false, true}},
        {UNI_HAL_ADC_STM32H7_CHANNEL_TEMPSENSOR, 18U, {false, false, true}},
        {UNI_HAL_ADC_STM32H7_CHANNEL_VBAT,       17U, {false, false, true}},
#else
        {UNI_HAL_ADC_STM32L4_CHANNEL_REFINT,      0U, {true, false, false}},
        {UNI_HAL_ADC_STM32L4_CHANNEL_TEMPSENSOR, 17U, {true, false, true}},
        {UNI_HAL_ADC_STM32L4_CHANNEL_VBAT,       18U, {true, false, true}},
#endif
    };
    static const uni_hal_core_periph_e instances[] = {
        UNI_HAL_CORE_PERIPH_ADC_1, UNI_HAL_CORE_PERIPH_ADC_2, UNI_HAL_CORE_PERIPH_ADC_3,
    };
    for (unsigned i = 0U; i < 3U; ++i) {
        for (unsigned j = 0U; j < sizeof routes / sizeof routes[0]; ++j) {
            reset_adc_model();
            uni_hal_adc_context_t ctx = {.config = {
                .instance = instances[i], .channels_count = 2U, .channels = {1U, routes[j].input},
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
                .clock_source = UNI_HAL_RCC_CLKSRC_PLL2P,
#endif
            }};
            if (!routes[j].routed[i]) {
                CHECK(!uni_hal_adc_init(&ctx));
                CHECK(!uni_hal_adc_is_inited(&ctx) && adc_model_untouched());
                // On L496 channel zero has no external input.
                if (routes[j].sequencer_channel == 0U) {
                    continue;
                }
                ctx.config.channels[1] = routes[j].sequencer_channel;
            }
            CHECK(uni_hal_adc_init(&ctx) && uni_hal_adc_is_inited(&ctx));
            const ADC_TypeDef *adc = adc_model_registers(instances[i]);
            CHECK(((adc->SQR1 & ADC_SQR1_SQ2) >> ADC_SQR1_SQ2_Pos) == routes[j].sequencer_channel);
        }
    }
    return 0U;
}
