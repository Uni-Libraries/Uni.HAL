//
// Includes
//

// Uni.Common
#include <uni_common.h>

// Uni.HAL
#include "adc/uni_hal_adc.h"



//
// Functions
//

bool uni_hal_adc_is_inited(const uni_hal_adc_context_t *ctx) {
    return ctx != NULL && ctx->state.initialized != false;
}



uint16_t uni_hal_adc_get_rank_raw(const uni_hal_adc_context_t *ctx, uint32_t rank) {
    uint16_t result = UINT16_MAX;
    if (uni_hal_adc_is_inited(ctx) && rank < ctx->config.channels_count) {
        const volatile uint16_t *array = ctx->config.data;
        if (array != NULL) {
            result = array[rank];
        }
    }
    return result;
}


uint16_t uni_hal_adc_get_channel_raw(const uni_hal_adc_context_t *ctx, uint32_t channel_idx) {
    uint16_t result = INT16_MAX;

    if (uni_hal_adc_is_inited(ctx)) {
        for (size_t rank_idx = 0; rank_idx < uni_common_math_min(ctx->config.channels_count, UNI_HAL_ADC_CHANNELS_MAX);
             rank_idx++) {
            if (ctx->config.channels[rank_idx] == channel_idx) {
                const volatile uint16_t *array = ctx->config.data;
                if (array != NULL) {
                    result = array[rank_idx];
                    break;
                }
            }
        }
    }

    return result;
}
