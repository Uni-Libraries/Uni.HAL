#include "crc/uni_hal_crc.h"

bool uni_hal_crc_init(uni_hal_crc_context_t *ctx) {
    if (!ctx) {
        return false;
    }

    ctx->initialized = false;
    switch (ctx->polynomial_size) {
    case UNI_HAL_CRC_POLYSIZE_7B:
    case UNI_HAL_CRC_POLYSIZE_8B:
    case UNI_HAL_CRC_POLYSIZE_16B:
    case UNI_HAL_CRC_POLYSIZE_32B:
        break;
    default:
        return false;
    }
    if ((ctx->polynomial_coef & 1U) == 0U) {
        return false;
    }

    // Left-align the LFSR so every width consumes the next input bit at bit 31.
    ctx->pc.shift = 32U - (uint32_t)ctx->polynomial_size;
    ctx->pc.polynomial = ctx->polynomial_coef << ctx->pc.shift;
    ctx->pc.remainder = ctx->initial_data << ctx->pc.shift;
    ctx->initialized = true;
    return true;
}

bool uni_hal_crc_append(uni_hal_crc_context_t *ctx, const uint8_t *data, uint32_t data_len) {
    if (!uni_hal_crc_is_inited(ctx) || !data) {
        return false;
    }
    uint32_t remainder = ctx->pc.remainder;
    for (uint32_t i = 0; i < data_len; ++i) {
        remainder ^= (uint32_t)data[i] << 24U;
        for (unsigned bit = 0; bit < 8U; ++bit) {
            uint32_t const feedback = (remainder >> 31U) ? ctx->pc.polynomial : 0U;
            remainder = (remainder << 1U) ^ feedback;
        }
    }
    ctx->pc.remainder = remainder;
    return true;
}

uint32_t uni_hal_crc_calculate(uni_hal_crc_context_t *ctx, const uint8_t *data, uint32_t data_len) {
    return ctx ? uni_hal_crc_calculate_ex(ctx, ctx->initial_data, data, data_len) : 0U;
}

uint32_t uni_hal_crc_calculate_ex(uni_hal_crc_context_t *ctx, uint32_t initial_value,
                                  const uint8_t *data, uint32_t data_len) {
    if (!data || !uni_hal_crc_reset_ex(ctx, initial_value)) {
        return 0U;
    }
    uni_hal_crc_append(ctx, data, data_len);
    return uni_hal_crc_get_value(ctx);
}

uint32_t uni_hal_crc_get_value(const uni_hal_crc_context_t *ctx) {
    return uni_hal_crc_is_inited(ctx) ? ctx->pc.remainder >> ctx->pc.shift : 0U;
}

bool uni_hal_crc_reset(uni_hal_crc_context_t *ctx) {
    return ctx && uni_hal_crc_reset_ex(ctx, ctx->initial_data);
}

bool uni_hal_crc_reset_ex(uni_hal_crc_context_t *ctx, uint32_t value) {
    if (!uni_hal_crc_is_inited(ctx)) {
        return false;
    }
    ctx->pc.remainder = value << ctx->pc.shift;
    return true;
}
