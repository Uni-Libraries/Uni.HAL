//
// Includes
//

// stdlib
#include <stddef.h>

// STM32 LL
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#include <stm32h7xx_ll_crc.h>
#pragma GCC diagnostic pop

// Uni.Common
#include <uni_common.h>

// uni_hal
#include "crc/uni_hal_crc.h"
#include "rcc/uni_hal_rcc.h"



//
// Private
//

static uint32_t _uni_hal_crc_get_polysize(uni_hal_crc_polysize_e polysize) {
    uint32_t result = 0;
    switch (polysize) {
    case UNI_HAL_CRC_POLYSIZE_7B:
        result = LL_CRC_POLYLENGTH_7B;
        break;
    case UNI_HAL_CRC_POLYSIZE_8B:
        result = LL_CRC_POLYLENGTH_8B;
        break;
    case UNI_HAL_CRC_POLYSIZE_16B:
        result = LL_CRC_POLYLENGTH_16B;
        break;
    case UNI_HAL_CRC_POLYSIZE_32B:
        result = LL_CRC_POLYLENGTH_32B;
        break;
    default:
        // unknown poly size
        break;
    }

    return result;
}


//
// Public
//


bool uni_hal_crc_init(uni_hal_crc_context_t *ctx) {
    bool result = false;
    if (ctx != NULL) {
        // enable clock
        result = uni_hal_rcc_clk_set(ctx->instance, true);

        if (result) {
            // initialize
            LL_CRC_SetInputDataReverseMode(CRC, LL_CRC_INDATA_REVERSE_NONE);
            LL_CRC_SetOutputDataReverseMode(CRC, LL_CRC_OUTDATA_REVERSE_NONE);
            LL_CRC_SetInitialData(CRC, ctx->initial_data);
            LL_CRC_SetPolynomialCoef(CRC, ctx->polynomial_coef);
            LL_CRC_SetPolynomialSize(CRC, _uni_hal_crc_get_polysize(ctx->polynomial_size));
            ctx->initialized = true;
        }
    }

    return result;
}


bool uni_hal_crc_append(uni_hal_crc_context_t *ctx, const uint8_t *data, uint32_t data_len) {
    bool result = false;
    if (uni_hal_crc_is_inited(ctx) && data != nullptr) {
        size_t idx;

        if ((size_t) data % 4 == 0) {
            // optimized path for aligned data
            for (idx = 0; idx < data_len / 4; idx++) {
                LL_CRC_FeedData32(CRC, uni_common_bytes_swap32(((const uint32_t *) data)[idx]));
            }
        } else {
            // common path for unaligned data
            for (idx = 0; idx < data_len / 4; idx++) {
                const uint32_t val =
                        (data[4U * idx] << 24U) | (data[(4U * idx) + 1U] << 16U) | (data[(4U * idx) + 2U] << 8U) |
                        data[(4U * idx) + 3U];
                LL_CRC_FeedData32(CRC, val);
            }
        }

        switch (data_len % 4) {
            case 1:
                LL_CRC_FeedData8(CRC, data[4 * idx]);
                break;
            case 2:
                LL_CRC_FeedData16(CRC, (data[4 * idx] << 8U) | data[4 * idx + 1]);
                break;
            case 3:
                LL_CRC_FeedData16(CRC, (data[4 * idx] << 8U) | data[4 * idx + 1]);
                LL_CRC_FeedData8(CRC, data[4 * idx + 2]);
                break;
            default:
                break;
        }

        result = true;
    }

    return  result;
}


uint32_t uni_hal_crc_calculate(uni_hal_crc_context_t *ctx, const uint8_t *data, uint32_t data_len) {
    uint32_t result = 0U;
    if (uni_hal_crc_is_inited(ctx) && data != NULL) {
        uni_hal_crc_reset(ctx);
        uni_hal_crc_append(ctx, data, data_len);
        result = uni_hal_crc_get_value(ctx);
    }

    return result;
}


uint32_t uni_hal_crc_calculate_ex(uni_hal_crc_context_t *ctx, uint32_t initial_value, const uint8_t *data, uint32_t data_len) {
    uint32_t result = 0U;
    if (uni_hal_crc_is_inited(ctx) && data != NULL) {
        uni_hal_crc_reset_ex(ctx, initial_value);
        uni_hal_crc_append(ctx, data, data_len);
        result = uni_hal_crc_get_value(ctx);
    }

    return result;
}


uint32_t uni_hal_crc_get_value(const uni_hal_crc_context_t *ctx) {
    uint32_t result = 0U;
    if (uni_hal_crc_is_inited(ctx)) {
        switch (ctx->polynomial_size) {
            case UNI_HAL_CRC_POLYSIZE_7B:
                result = LL_CRC_ReadData7(CRC);
                break;
            case UNI_HAL_CRC_POLYSIZE_8B:
                result = LL_CRC_ReadData8(CRC);
                break;
            case UNI_HAL_CRC_POLYSIZE_16B:
                result = LL_CRC_ReadData16(CRC);
                break;
            case UNI_HALфдшу_CRC_POLYSIZE_32B:
                result = LL_CRC_ReadData32(CRC);
                break;
            default:
                // unknown poly size
                break;
        }
    }

    return result;
}

bool uni_hal_crc_reset(uni_hal_crc_context_t *ctx) {
    bool result = false;
    if (uni_hal_crc_is_inited(ctx)) {
        LL_CRC_SetInitialData(CRC, ctx->initial_data);
        result = true;
    }

    return result;
}


bool uni_hal_crc_reset_ex(uni_hal_crc_context_t *ctx, uint32_t value) {
    bool result = false;
    if (uni_hal_crc_is_inited(ctx)) {
        LL_CRC_SetInitialData(CRC, value);
        result = true;
    }

    return result;
}
