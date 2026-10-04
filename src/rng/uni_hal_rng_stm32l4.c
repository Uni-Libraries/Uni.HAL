//
// Includes
//

// stdlib
#include <stddef.h>
#include <string.h>

// ST
#include <stm32l496xx.h>
#include <stm32l4xx_ll_rng.h>

// Uni.Common
#include <uni_common.h>

// Uni.HAL
#include "rcc/uni_hal_rcc.h"
#include "rng/uni_hal_rng.h"
#include "systick/uni_hal_systick.h"


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


enum {
    /** longest wait for one random word */
    UNI_HAL_RNG_TIMEOUT_MS = 100U,
};

/**
 * Read one random word once the generator has it ready
 * @param instance RNG instance
 * @param out receives the random word
 * @return false when no valid word showed up in time (no kernel clock, seed or clock error)
 */
static bool _uni_hal_rng_read_32u(RNG_TypeDef *instance, uint32_t *out) {
    uint32_t const start_ms = uni_hal_systick_get_ms();

    for (;;) {
        if (LL_RNG_IsActiveFlag_SEIS(instance) != 0U) {
            // seed error: the data on hand must not be used, restart the generator
            LL_RNG_ClearFlag_SEIS(instance);
            LL_RNG_Disable(instance);
            LL_RNG_Enable(instance);
        }
        else if (LL_RNG_IsActiveFlag_DRDY(instance) != 0U) {
            break;
        }

        if (LL_RNG_IsActiveFlag_CEIS(instance) != 0U) {
            LL_RNG_ClearFlag_CEIS(instance);
        }

        if ((uni_hal_systick_get_ms() - start_ms) > UNI_HAL_RNG_TIMEOUT_MS) {
            return false;
        }
    }

    uint32_t const value = LL_RNG_ReadRandData32(instance);
    *out = value;
    return true;
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
            // result stays 0 when the generator does not deliver
            (void)_uni_hal_rng_read_32u(instance, &result);
        }
    }
    return result;
}


bool uni_hal_rng_get(uni_hal_rng_context_t *ctx, uint8_t* buf, size_t buf_len) {
    bool result = false;

    if (buf != NULL && uni_hal_rng_is_inited(ctx)) {
        RNG_TypeDef *instance = _uni_hal_rng_get_instance(ctx->instance);
        if (instance != NULL) {
            result = true;

            size_t offset = 0U;
            while (result && offset < buf_len) {
                uint32_t rng_val = 0U;
                result = _uni_hal_rng_read_32u(instance, &rng_val);
                if (result) {
                    size_t const chunk = uni_common_math_min(buf_len - offset, sizeof(rng_val));
                    memcpy(&buf[offset], &rng_val, chunk);
                    offset += chunk;
                }
            }
        }
    }
    return result;
}
