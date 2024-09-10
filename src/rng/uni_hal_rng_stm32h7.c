//
// Includes
//

// stdlib
#include <stddef.h>
#include <string.h>

// ST
#include <stm32h7xx_ll_rng.h>

// UNI_HAL
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
    RNG_TypeDef *result;
    switch (instance) {
        case UNI_HAL_CORE_PERIPH_RNG:
            result = RNG; //-V2571
            break;
        default:
            result = nullptr;
            break;
    }
    return result;
}


//
// Functions
//

bool uni_hal_rng_init(uni_hal_rng_context_t *ctx) {
    bool result = false;

    if (ctx != nullptr) {
        RNG_TypeDef *instance = _uni_hal_rng_get_instance(ctx->instance);
        if (instance != NULL) {
            //TODO: make it configurable
            result = uni_hal_rcc_clksrc_set(UNI_HAL_CORE_PERIPH_RNG, UNI_HAL_RCC_CLKSRC_PLL1Q);
            result = result && uni_hal_rcc_clk_set(UNI_HAL_CORE_PERIPH_RNG, true);
            if (result) {
                LL_RNG_Enable(instance);
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


uint8_t uni_hal_rng_get_8u(uni_hal_rng_context_t *ctx) { return (uint8_t)(uni_hal_rng_get_32u(ctx) & 0xFFU); }


uint16_t uni_hal_rng_get_16u(uni_hal_rng_context_t *ctx) { return (uint16_t)(uni_hal_rng_get_32u(ctx) & 0xFFFFU); }


uint32_t uni_hal_rng_get_32u(uni_hal_rng_context_t *ctx) {
    uint32_t result = 0U;
    if (uni_hal_rng_is_inited(ctx)) {
        RNG_TypeDef *instance = _uni_hal_rng_get_instance(ctx->instance);
        if (instance != NULL) {
            // wait for result
            while (LL_RNG_IsActiveFlag_DRDY(instance) == 0U) {}

            // get result
            result = LL_RNG_ReadRandData32(instance);
        }
    }
    return result;
}


bool uni_hal_rng_get(uni_hal_rng_context_t *ctx, uint8_t* buf, size_t buf_len) {
    bool result = false;

    if (buf != nullptr && uni_hal_rng_is_inited(ctx)) {
        RNG_TypeDef *instance = _uni_hal_rng_get_instance(ctx->instance);
        if (instance != NULL) {
            size_t offset;
            uint32_t *buf_32 = (uint32_t *)buf;
            for (offset = 0U; offset < buf_len / sizeof(uint32_t); offset++) {
                buf_32[offset] = LL_RNG_ReadRandData32(instance);
            }

            uint32_t rng_val = LL_RNG_ReadRandData32(instance);
            memcpy(&buf[offset * sizeof(uint32_t)], &rng_val, buf_len % sizeof(uint32_t));

            result = true;
        }
    }
    return result;
}
