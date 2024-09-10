//
// Include
//

// uni_hal
#include "adc/uni_hal_adc.h"



//
// Functions
//

bool uni_hal_adc_init(uni_hal_adc_context_t *ctx){
    bool result = false;
    if(ctx!=NULL){
        ctx->state.initialized = true;
        result = true;
    }
    return result;
}

bool uni_hal_adc_trigger(uni_hal_adc_context_t* ctx){
    bool result = false;
    if(uni_hal_adc_is_inited(ctx)){
        result = true;
    }
    return result;
}


uint16_t uni_hal_adc_get_rank_raw(const uni_hal_adc_context_t *ctx, uint32_t rank){
    uint16_t result = 0U;
    if(uni_hal_adc_is_inited(ctx)){
        (void)rank;
        result = 1U;
    }
    return result;
}


uint16_t uni_hal_adc_get_channel_raw(const uni_hal_adc_context_t *ctx, uint32_t channel){
    uint16_t result = 0U;
    if(uni_hal_adc_is_inited(ctx)){
        (void)channel;
        result = 1U;
    }
    return result;
}


float uni_hal_adc_get_channel_voltage(const uni_hal_adc_context_t *ctx, uint32_t channel){
    float result = 0.0f;
    if(uni_hal_adc_is_inited(ctx)){
        (void)channel;
        result = 1.0f;
    }
    return result;
}

uint16_t uni_hal_adc_get_channel_mv(const uni_hal_adc_context_t *ctx, uint32_t channel){
    uint32_t result = 0U;
    if(uni_hal_adc_is_inited(ctx)){
        (void)channel;
        result = 1U;
    }
    return result;
}


float uni_hal_adc_mcutemp_convert(const uni_hal_adc_context_t *ctx, uint16_t raw_val){
    float result = 0.0f;
    if(uni_hal_adc_is_inited(ctx)){
        result = raw_val;
    }
    return result;
}


float uni_hal_adc_mcutemp_get(const uni_hal_adc_context_t *ctx){
    float result = 0.0f;
    if(uni_hal_adc_is_inited(ctx)){
        result = 1.0f;
    }
    return result;
}


uint16_t uni_hal_adc_mcutemp_raw(const uni_hal_adc_context_t *ctx){
    uint16_t result = 0U;
    if(uni_hal_adc_is_inited(ctx)){
        result = 1U;
    }
    return result;
}
