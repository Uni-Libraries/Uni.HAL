#include "hash/uni_hal_hash.h"

#if defined(UNI_HAL_TARGET_MCU_STM32H753)

#include <string.h>

#include <stm32h7xx_hal.h>

#include "core/uni_hal_core.h"

/*
 * RM0433, HASH context swapping in polling mode. The first 16 words fill
 * the FIFO. Word 17 launches the first compression. Each subsequent 16 words
 * reach the next save point. Reading a CSR before DINIS is set returns zero.
 */
enum { block_size = 64, first_transfer = 68, poll_limit = 4096 };

bool uni_hal_hash_start(uni_hal_hash_context_t *ctx, uni_hal_hash_algorithm_e algorithm) {
    if (ctx == nullptr) {
        return false;
    }
    *ctx = (uni_hal_hash_context_t){0};
    switch (algorithm) {
    case UNI_HAL_HASH_MD5:
        ctx->control = HASH_ALGOSELECTION_MD5;
        ctx->digest_size = 16;
        break;
    case UNI_HAL_HASH_SHA1:
        ctx->control = HASH_ALGOSELECTION_SHA1;
        ctx->digest_size = 20;
        break;
    case UNI_HAL_HASH_SHA224:
        ctx->control = HASH_ALGOSELECTION_SHA224;
        ctx->digest_size = 28;
        break;
    case UNI_HAL_HASH_SHA256:
        ctx->control = HASH_ALGOSELECTION_SHA256;
        ctx->digest_size = 32;
        break;
    default:
        return false;
    }
    ctx->control |= HASH_DATATYPE_8B;
    return true;
}

static bool wait_status(uint32_t mask, uint32_t value) {
    // Each transaction computes at most two compression blocks. Use a bounded
    // poll independent of SysTick, which cannot advance in the critical section.
    for (unsigned i = 0; i < poll_limit; ++i) {
        if ((HASH->SR & mask) == value) {
            return true;
        }
    }
    return false;
}

static void write_input(const uint8_t *data, size_t len) {
    while (len != 0) {
        size_t count = len < sizeof(uint32_t) ? len : sizeof(uint32_t);
        uint32_t word = 0;
        memcpy(&word, data, count);
        HASH->DIN = word;
        data += count;
        len -= count;
    }
}

static bool transfer(uni_hal_hash_context_t *ctx, uint8_t *digest) {
    // Limit the masked interval to a single FIFO transfer. No task or interrupt
    // can preempt between programming the peripheral and saving this context.
    uint32_t irq_mask = uni_hal_core_irq_pause();
    __HAL_RCC_HASH_CLK_ENABLE();
    HASH->IMR = 0;
    HASH->STR = 0;
    HASH->CR = ctx->control;
    HASH->CR |= HASH_CR_INIT;
    if (ctx->saved) {
        for (size_t i = 0; i < sizeof(ctx->csr) / sizeof(ctx->csr[0]); ++i) {
            HASH->CSR[i] = ctx->csr[i];
        }
    }

    bool ready;
    if (digest == nullptr) {
        write_input(ctx->pending, ctx->pending_len);
        ready = wait_status(HASH_SR_BUSY | HASH_SR_DINIS, HASH_SR_DINIS);
        if (ready) {
            for (size_t i = 0; i < sizeof(ctx->csr) / sizeof(ctx->csr[0]); ++i) {
                ctx->csr[i] = HASH->CSR[i];
            }
            ctx->saved = true;
        }
    } else {
        // NBLW == 0 means all 32 bits of the last word are valid.
        HASH->STR = (uint32_t)(ctx->pending_len % sizeof(uint32_t)) * 8u;
        write_input(ctx->pending, ctx->pending_len);
        HASH->STR |= HASH_STR_DCAL;
        ready = wait_status(HASH_SR_DCIS, HASH_SR_DCIS);
        if (ready) {
            for (size_t i = 0; i < ctx->digest_size / sizeof(uint32_t); ++i) {
                uint32_t word = __REV(HASH_DIGEST->HR[i]);
                memcpy(digest + i * sizeof(word), &word, sizeof(word));
            }
        }
    }

    // Reset also clears intermediate key-derived data and recovers a timed-out unit.
    __HAL_RCC_HASH_FORCE_RESET();
    __HAL_RCC_HASH_RELEASE_RESET();
    __HAL_RCC_HASH_CLK_DISABLE();
    uni_hal_core_irq_resume(irq_mask);
    ctx->pending_len = 0;
    if (!ready || digest != nullptr) {
        *ctx = (uni_hal_hash_context_t){0};
    }
    return ready;
}

bool uni_hal_hash_update(uni_hal_hash_context_t *ctx, const uint8_t *data, size_t len) {
    if (ctx == nullptr || ctx->digest_size == 0 || (data == nullptr && len != 0)) {
        return false;
    }
    while (len != 0) {
        size_t target = ctx->saved ? block_size : first_transfer;
        size_t remaining = target - ctx->pending_len;
        size_t count = len < remaining ? len : remaining;
        memcpy(ctx->pending + ctx->pending_len, data, count);
        ctx->pending_len += count;
        data += count;
        len -= count;
        if (ctx->pending_len == target && !transfer(ctx, nullptr)) {
            return false;
        }
    }
    return true;
}

bool uni_hal_hash_finish(uni_hal_hash_context_t *ctx, uint8_t *digest, size_t capacity) {
    return ctx != nullptr && ctx->digest_size != 0 && digest != nullptr &&
           capacity >= ctx->digest_size && transfer(ctx, digest);
}

#endif
