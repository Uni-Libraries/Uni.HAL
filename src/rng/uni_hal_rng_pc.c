//
// Includes
//

#include "rng/uni_hal_rng.h"



//
// Functions
//

bool uni_hal_rng_init(uni_hal_rng_context_t *ctx){
    bool result = false;
    if(ctx!=NULL){
        ctx->inited = true;
        result = true;
    }
    return result;
}


bool uni_hal_rng_is_inited(const uni_hal_rng_context_t *ctx){
    bool result = false;
    if(ctx!=NULL){
        result = ctx->inited;
    }
    return result;
}


uint8_t uni_hal_rng_get_8u(uni_hal_rng_context_t *ctx) {
    uint8_t result = 0U;
    if (uni_hal_rng_is_inited(ctx)) {
        result = 1U;
    }
    return result;
}


uint16_t uni_hal_rng_get_16u(uni_hal_rng_context_t *ctx){
    uint16_t result = 0U;
    if (uni_hal_rng_is_inited(ctx)) {
        result = 1U;
    }
    return result;
}


uint32_t uni_hal_rng_get_32u(uni_hal_rng_context_t *ctx){
    uint32_t result = 0U;
    if (uni_hal_rng_is_inited(ctx)) {
        result = 1U;
    }
    return result;
}


bool uni_hal_rng_get(uni_hal_rng_context_t *ctx, uint8_t *buf, size_t buf_len){
    bool result = false;
    if (uni_hal_rng_is_inited(ctx)) {
        (void)buf;
        (void)buf_len;
        result = true;
    }
    return result;
}
