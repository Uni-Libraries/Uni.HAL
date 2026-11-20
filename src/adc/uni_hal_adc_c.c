//
// Includes
//

// Uni.Common
#include <uni_common.h>

// Uni.HAL
#include "adc/uni_hal_adc.h"
#include "adc/uni_hal_adc_internal.h"
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include "dma/uni_hal_dma_stm32h7_irq.h"
#endif



//
// Functions
//

bool uni_hal_adc_is_inited(const uni_hal_adc_context_t *ctx) {
    if (ctx == NULL || !ctx->state.initialized) {
        return false;
    }
#if defined(UNI_HAL_TARGET_MCU_STM32L496)
    if (ctx->config.dma && uni_hal_dma_stm32l4_has_transfer_error(ctx->config.dma)) {
        return false;
    }
#elif defined(UNI_HAL_TARGET_MCU_STM32H743)
    if (ctx->config.dma) {
        return uni_hal_dma_stm32h7_circular_active(ctx->config.dma, ctx);
    }
#endif
    return true;
}



uint16_t uni_hal_adc_get_rank_raw(const uni_hal_adc_context_t *ctx, uint32_t rank) {
    uint16_t result = UINT16_MAX;
    if (uni_hal_adc_is_inited(ctx) && rank < UNI_HAL_ADC_CHANNELS_MAX && rank < ctx->config.channels_count) {
        const volatile uint16_t *array = ctx->config.data;
        if (array != NULL) {
            result = array[rank];
        }
    }
    return result;
}


bool uni_hal_adc_find_sample(const uni_hal_adc_context_t *ctx, uint32_t channel, uint16_t *sample) {
    if (uni_hal_adc_is_inited(ctx) && ctx->config.data != NULL) {
        for (size_t rank = 0; rank < uni_common_math_min(ctx->config.channels_count, UNI_HAL_ADC_CHANNELS_MAX); ++rank) {
            if (ctx->config.channels[rank] == channel) {
                if (sample != NULL) {
                    *sample = ctx->config.data[rank];
                }
                return true;
            }
        }
    }
    return false;
}

bool uni_hal_adc_has_channel(const uni_hal_adc_context_t *ctx, uint32_t channel) {
    return uni_hal_adc_find_sample(ctx, channel, NULL);
}

uint16_t uni_hal_adc_get_channel_raw(const uni_hal_adc_context_t *ctx, uint32_t channel) {
    uint16_t sample = UINT16_MAX;
    uni_hal_adc_find_sample(ctx, channel, &sample);
    return sample;
}
