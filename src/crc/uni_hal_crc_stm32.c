#include <string.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include <stm32h7xx_ll_crc.h>
#else
#include <stm32l4xx_ll_crc.h>
#endif
#pragma GCC diagnostic pop

#include <uni_common.h>
#include "crc/uni_hal_crc.h"
#include "rcc/uni_hal_rcc.h"

bool uni_hal_crc_init(uni_hal_crc_context_t *ctx) {
    if (ctx == nullptr) {
        return false;
    }
    ctx->initialized = false;
    switch (ctx->polynomial_size) {
    case UNI_HAL_CRC_POLYSIZE_7B:
        ctx->stm32.length = LL_CRC_POLYLENGTH_7B;
        break;
    case UNI_HAL_CRC_POLYSIZE_8B:
        ctx->stm32.length = LL_CRC_POLYLENGTH_8B;
        break;
    case UNI_HAL_CRC_POLYSIZE_16B:
        ctx->stm32.length = LL_CRC_POLYLENGTH_16B;
        break;
    case UNI_HAL_CRC_POLYSIZE_32B:
        ctx->stm32.length = LL_CRC_POLYLENGTH_32B;
        break;
    default:
        return false;
    }
    if (ctx->instance != UNI_HAL_CORE_PERIPH_CRC_1 || !(ctx->polynomial_coef & 1U) ||
        !uni_hal_rcc_clk_set(ctx->instance, true)) {
        return false;
    }
    ctx->stm32.mask = UINT32_MAX >> (32U - (uint32_t)ctx->polynomial_size);
    ctx->stm32.polynomial = ctx->polynomial_coef;
    ctx->stm32.remainder = ctx->initial_data & ctx->stm32.mask;
    ctx->initialized = true;
    return true;
}

static void feed_bytes(const uint8_t *data, uint32_t len) {
    // Aligned word writes feed the most significant byte first. Keep byte order at both edges.
    while (len && (uintptr_t)data % sizeof(uint32_t) != 0U) {
        LL_CRC_FeedData8(CRC, *data++);
        --len;
    }
    while (len >= sizeof(uint32_t)) {
        uint32_t word;
        memcpy(&word, data, sizeof(word));
        LL_CRC_FeedData32(CRC, uni_common_bytes_swap32(word));
        data += sizeof(word);
        len -= sizeof(word);
    }
    while (len--) {
        LL_CRC_FeedData8(CRC, *data++);
    }
}

static uint32_t update(uni_hal_crc_context_t *ctx, uint32_t seed, const uint8_t *data, uint32_t len) {
    uint32_t remainder = seed & ctx->stm32.mask;
    // Share the single hardware unit with other tasks and IRQs at bounded intervals.
    enum { chunk_size = 64U };
    while (len > 0U) {
        uint32_t chunk = len < chunk_size ? len : chunk_size;
        uint32_t irq_mask = uni_hal_core_irq_pause();
        LL_CRC_SetInputDataReverseMode(CRC, LL_CRC_INDATA_REVERSE_NONE);
        LL_CRC_SetOutputDataReverseMode(CRC, LL_CRC_OUTDATA_REVERSE_NONE);
        LL_CRC_SetPolynomialSize(CRC, ctx->stm32.length);
        LL_CRC_SetPolynomialCoef(CRC, ctx->stm32.polynomial);
        LL_CRC_SetInitialData(CRC, remainder);
        LL_CRC_ResetCRCCalculationUnit(CRC);
        feed_bytes(data, chunk);
        remainder = LL_CRC_ReadData32(CRC) & ctx->stm32.mask;
        uni_hal_core_irq_resume(irq_mask);
        data += chunk;
        len -= chunk;
    }
    ctx->stm32.remainder = remainder;
    return remainder;
}

bool uni_hal_crc_append(uni_hal_crc_context_t *ctx, const uint8_t *data, uint32_t data_len) {
    if (!uni_hal_crc_is_inited(ctx) || data == nullptr) {
        return false;
    }
    update(ctx, ctx->stm32.remainder, data, data_len);
    return true;
}

uint32_t uni_hal_crc_calculate(uni_hal_crc_context_t *ctx, const uint8_t *data, uint32_t data_len) {
    return ctx ? uni_hal_crc_calculate_ex(ctx, ctx->initial_data, data, data_len) : 0U;
}

uint32_t uni_hal_crc_calculate_ex(uni_hal_crc_context_t *ctx, uint32_t initial_value, const uint8_t *data, uint32_t data_len) {
    return uni_hal_crc_is_inited(ctx) && data ? update(ctx, initial_value, data, data_len) : 0U;
}

uint32_t uni_hal_crc_get_value(const uni_hal_crc_context_t *ctx) {
    return uni_hal_crc_is_inited(ctx) ? ctx->stm32.remainder : 0U;
}

bool uni_hal_crc_reset(uni_hal_crc_context_t *ctx) {
    return ctx && uni_hal_crc_reset_ex(ctx, ctx->initial_data);
}

bool uni_hal_crc_reset_ex(uni_hal_crc_context_t *ctx, uint32_t value) {
    if (!uni_hal_crc_is_inited(ctx)) {
        return false;
    }
    ctx->stm32.remainder = value & ctx->stm32.mask;
    return true;
}
