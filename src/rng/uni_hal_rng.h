#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//
// Includes
//

// stdlib
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// uni_hal
#include "core/uni_hal_core.h"
#include "rcc/uni_hal_rcc_enum.h"


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
     * Kernel clock of the generator.
     * UNI_HAL_RCC_CLKSRC_UNKNOWN (0) keeps the former behaviour: PLL1Q on the STM32H7, the source
     * selected by the RCC driver on the STM32L4. UNI_HAL_RCC_CLKSRC_HSI48 also starts the HSI48
     * oscillator, so the generator does not depend on the PLL configuration.
     */
    uni_hal_rcc_clksrc_e clock_source;

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
 * @return random value, 0 when the generator does not deliver one in time
 */
uint32_t uni_hal_rng_get_32u(uni_hal_rng_context_t *ctx);

/**
 * Fills given buffer with a random values
 * @param ctx pointer to the RNG context
 * @param buf pointer to the buffer which should be filled with random values
 * @param buf_len buffer size
 * @return true on success, false when the generator does not deliver in time
 */
bool uni_hal_rng_get(uni_hal_rng_context_t *ctx, uint8_t *buf, size_t buf_len);

#if defined(__cplusplus)
}
#endif
