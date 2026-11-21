#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(UNI_HAL_TARGET_MCU_STM32H753)

/** Algorithms implemented by the STM32H753 HASH peripheral. */
typedef enum {
    UNI_HAL_HASH_INVALID,
    UNI_HAL_HASH_MD5,
    UNI_HAL_HASH_SHA1,
    UNI_HAL_HASH_SHA224,
    UNI_HAL_HASH_SHA256,
} uni_hal_hash_algorithm_e;

/**
 * Storage for one incremental hash. Treat the fields as private.
 * Independent contexts can be interleaved, including across tasks. Serialize
 * accesses to the same context. This driver owns the on-chip HASH peripheral.
 */
typedef struct {
    uint32_t csr[38];
    uint8_t pending[68];
    size_t pending_len;
    uint32_t control;
    size_t digest_size;
    bool saved;
} uni_hal_hash_context_t;

/** Start a new message in caller-owned storage, discarding any previous state. */
bool uni_hal_hash_start(uni_hal_hash_context_t *ctx, uni_hal_hash_algorithm_e algorithm);

/** Append bytes, including unaligned input. A zero-length update accepts a null data pointer. */
bool uni_hal_hash_update(uni_hal_hash_context_t *ctx, const uint8_t *data, size_t len);

/**
 * Write the digest in standard byte order and end the operation.
 * An undersized output leaves the context usable. A peripheral timeout
 * invalidates the context. Call start again for the next message.
 */
bool uni_hal_hash_finish(uni_hal_hash_context_t *ctx, uint8_t *digest, size_t capacity);

#endif

#if defined(__cplusplus)
}
#endif
