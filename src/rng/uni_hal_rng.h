#pragma once

//
// Includes
//

// stdlib
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// uni_hal
#include "core/uni_hal_core.h"


//
// Typedefs
//

/**
 * RNG module context
 */
typedef struct {
    /**
     * RNG Instance
     */
    uni_hal_core_periph_e instance;

    /**
     * Inited flag
     */
    bool inited;

} uni_hal_rng_context_t;


//
// Functions
//

/**
 * Initializes RNG hardware block
 * @param ctx pointer to the RNG context
 * @return true on success
 */
bool uni_hal_rng_init(uni_hal_rng_context_t *ctx);

/**
 * Checks that RNG was properly inited
 * @param ctx pointer to the RNG context
 * @return true on success
 */
bool uni_hal_rng_is_inited(const uni_hal_rng_context_t *ctx);

/**
 * Receives one byte of random values
 * @param ctx pointer to the RNG context
 * @return random value
 */
uint8_t uni_hal_rng_get_8u(uni_hal_rng_context_t *ctx);

/**
 * Receives two bytes of random values
 * @param ctx pointer to the RNG context
 * @return random value
 */
uint16_t uni_hal_rng_get_16u(uni_hal_rng_context_t *ctx);

/**
 * Receives four bytes of random values
 * @param ctx pointer to the RNG context
 * @return random value
 */
uint32_t uni_hal_rng_get_32u(uni_hal_rng_context_t *ctx);

/**
 * Fills given buffer with a random values
 * @param ctx pointer to the RNG context
 * @param buf pointer to the buffer which should be filled with random values
 * @param buf_len buffer size
 * @return true on success
 */
bool uni_hal_rng_get(uni_hal_rng_context_t *ctx, uint8_t *buf, size_t buf_len);
