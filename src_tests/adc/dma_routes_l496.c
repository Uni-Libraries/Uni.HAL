#include "model.h"

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (false)

unsigned test_adc(void) {
    // STM32L496 RM0351, tables 44 and 45: DMA request zero has fixed wiring.
    static const uni_hal_dma_channel_e routes[2][3] = {
        {UNI_HAL_DMA_CHANNEL_1, UNI_HAL_DMA_CHANNEL_2, UNI_HAL_DMA_CHANNEL_3},
        {UNI_HAL_DMA_CHANNEL_3, UNI_HAL_DMA_CHANNEL_4, UNI_HAL_DMA_CHANNEL_5},
    };
    for (unsigned controller = 0; controller < 2; ++controller) {
        for (unsigned adc = 0; adc < 3; ++adc) {
            for (unsigned channel = 1; channel <= 7; ++channel) {
                reset_adc_model();
                adc_dma_init_succeeds = true;
                uint16_t sample = 1234U;
                uni_hal_dma_context_t dma = {.config = {
                    .instance = controller == 0 ? UNI_HAL_CORE_PERIPH_DMA_1 : UNI_HAL_CORE_PERIPH_DMA_2,
                    .channel = (uni_hal_dma_channel_e)channel,
                }};
                uni_hal_adc_context_t ctx = {.config = {
                    .instance = (uni_hal_core_periph_e)(UNI_HAL_CORE_PERIPH_ADC_1 + adc),
                    .channels_count = 1U, .data = &sample, .dma = &dma,
                }};
                if (channel == routes[controller][adc]) {
                    CHECK(uni_hal_adc_init(&ctx) && uni_hal_adc_is_inited(&ctx));
                    CHECK(dma_init_calls == 1U);
                } else {
                    CHECK(!uni_hal_adc_init(&ctx));
                    CHECK(!uni_hal_adc_is_inited(&ctx) && !uni_hal_dma_is_inited(&dma));
                    CHECK(adc_model_untouched() && dma_init_calls == 0U);
                    CHECK(uni_hal_adc_get_rank_raw(&ctx, 0U) == UINT16_MAX);
                }
            }
        }
    }
    return 0;
}
