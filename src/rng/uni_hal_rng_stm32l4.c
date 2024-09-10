//
// Includes
//

// stdlib
#include <stddef.h>
#include <string.h>

// ST
#include <stm32l496xx.h>
#include <stm32l4xx_ll_rng.h>

// Uni.HAL
#include "rcc/uni_hal_rcc.h"
#include "rng/uni_hal_rng.h"


//
// Private
//

/**
 * Get RNG instance
 * @param instance instance enum
 * @return pointer to the instance
 */
static RNG_TypeDef *_uni_hal_rng_get_instance(uni_hal_core_periph_e instance) {
    RNG_TypeDef *result = NULL;
    switch (instance) {
    case UNI_HAL_CORE_PERIPH_RNG:
        result = RNG;
        break;
    default:
        break;
    }
    return result;
}


//
// Functions
//

bool uni_hal_rng_init(uni_hal_rng_context_t *ctx) {
    bool result = false;

    if (ctx != NULL) {
        RNG_TypeDef *instance = _uni_hal_rng_get_instance(ctx->instance);
        if (instance != NULL) {
            result = uni_hal_rcc_clk_set(UNI_HAL_CORE_PERIPH_RNG, true);
            if (result) {
                LL_RNG_Enable(instance);
                result = true;
                ctx->inited = true;
            }
        }
    }

    return result;
}

bool uni_hal_rng_is_inited(const uni_hal_rng_context_t *ctx) {
    bool result = false;
    if(ctx!=NULL){
        result = ctx->inited;
    }
    return result;
}


uint8_t uni_hal_rng_get_8u(uni_hal_rng_context_t *ctx) { return (uint8_t)(uni_hal_rng_get_32u(ctx) & UINT8_MAX); }


uint16_t uni_hal_rng_get_16u(uni_hal_rng_context_t *ctx) { return (uint16_t)(uni_hal_rng_get_32u(ctx) & UINT16_MAX); }


uint32_t uni_hal_rng_get_32u(uni_hal_rng_context_t *ctx) {
    uint32_t result = 0U;
    if (uni_hal_rng_is_inited(ctx)) {
        RNG_TypeDef *instance = _uni_hal_rng_get_instance(ctx->instance);
        if (instance != NULL) {
            result = LL_RNG_ReadRandData32(instance);
        }
    }
    return result;
}


bool uni_hal_rng_get(uni_hal_rng_context_t *ctx, uint8_t* buf, size_t buf_len) {
    bool result = false;

    if (uni_hal_rng_is_inited(ctx) && buf != NULL) {
        RNG_TypeDef *instance = _uni_hal_rng_get_instance(ctx->instance);
        if (instance != NULL) {
            size_t offset = 0;
            uint32_t *buf_32 = (uint32_t *)buf;
            for (offset = 0; offset < buf_len / sizeof(uint32_t); offset++) {
                buf_32[offset] = LL_RNG_ReadRandData32(instance);
            }

            uint32_t rng_val = LL_RNG_ReadRandData32(instance);
            memcpy(&buf[offset * sizeof(uint32_t)], &rng_val, buf_len % sizeof(uint32_t));

            result = true;
        }
    }
    return result;
}
