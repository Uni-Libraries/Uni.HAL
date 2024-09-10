//
// Includes
//

// stdlib
#include <stddef.h>

// uni_hal
#include "crc/uni_hal_crc.h"



//
// Functions
//

bool uni_hal_crc_init(uni_hal_crc_context_t *ctx){
    bool result = false;
    if(ctx != NULL){
        ctx->initialized = true;
        result = true;
    }
    return result;
}


bool uni_hal_crc_append(uni_hal_crc_context_t *ctx, const uint8_t *data, uint32_t data_len){
    bool result = false;
    if(uni_hal_crc_is_inited(ctx)){
        (void)data;
        (void)data_len;
        result = true;
    }
    return result;
}


uint32_t uni_hal_crc_calculate(uni_hal_crc_context_t *ctx, const uint8_t *data, uint32_t data_len){
    uint32_t result = 0U;
    if(uni_hal_crc_is_inited(ctx)){
        (void)data;
        (void)data_len;
        result = 1U;
    }
    return result;
}


uint32_t uni_hal_crc_calculate_ex(uni_hal_crc_context_t *ctx, uint32_t initial_value, const uint8_t *data, uint32_t data_len){
    uint32_t result = 0U;
    if(uni_hal_crc_is_inited(ctx)){
        (void)initial_value;
        (void)data;
        (void)data_len;
        result = 1U;
    }
    return result;
}


uint32_t uni_hal_crc_get_value(const uni_hal_crc_context_t *ctx){
    uint32_t result = 0U;
    if(uni_hal_crc_is_inited(ctx)){
        result = 1U;
    }
    return result;
}


bool uni_hal_crc_reset(uni_hal_crc_context_t *ctx){
    bool result = false;
    if(uni_hal_crc_is_inited(ctx)){
        result = true;
    }
    return result;
}


bool uni_hal_crc_reset_ex(uni_hal_crc_context_t *ctx, uint32_t value){
    uint32_t result = 0U;
    if(uni_hal_crc_is_inited(ctx)){
        (void)value;
        result = 1U;
    }
    return result;
}
