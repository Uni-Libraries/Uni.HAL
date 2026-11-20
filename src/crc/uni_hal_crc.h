#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//
// Includes
//

// stdlib
#include <stdbool.h>
#include <stdint.h>

// uni_hal
#include "core/uni_hal_core.h"



//
// Typedefs
//

/**
 * CRC polynomial size
 */
typedef enum {
    /**
     * CRC-7
     */
    UNI_HAL_CRC_POLYSIZE_7B = 7,

    /**
     * CRC-8
     */
    UNI_HAL_CRC_POLYSIZE_8B = 8,

    /**
     * CRC-16
     */
    UNI_HAL_CRC_POLYSIZE_16B = 16,

    /**
     * CRC-32
     */
    UNI_HAL_CRC_POLYSIZE_32B = 32,
} uni_hal_crc_polysize_e;


/**
 * Each initialized context owns its polynomial and running remainder. Different
 * contexts may be interleaved. Serialize access to the same context.
 */
typedef struct uni_hal_crc_context_t {
    /**
     * CRC instance
     */
    uni_hal_core_periph_e  instance;

    /**
     * Initial calculation value
     */
    uint32_t initial_data;

    /**
     * Polonomial coefficients in bitmask format
     */
    uint32_t polynomial_coef;

    /**
     * Polynomial size
     */
    uni_hal_crc_polysize_e polynomial_size;

    /**
     * Initialization state
     */
    bool initialized;

#if defined(UNI_HAL_TARGET_MCU_PC)
    /** Private state of the software calculation unit. */
    struct {
        uint32_t shift;
        uint32_t polynomial;
        uint32_t remainder;
#if defined(__cplusplus)
    } pc{};  // Allow client aggregates to initialize just the configuration and status.
#else
    } pc;
#endif
#elif defined(UNI_HAL_TARGET_MCU_STM32H743) || defined(UNI_HAL_TARGET_MCU_STM32L496)
    /** Private configuration and remainder saved between hardware transactions. */
    struct {
        uint32_t length;
        uint32_t polynomial;
        uint32_t mask;
        uint32_t remainder;
#if defined(__cplusplus)
    } stm32{};
#else
    } stm32;
#endif
#endif

} uni_hal_crc_context_t;


//
// Functions
//

/**
 * Initializes CRC module
 * @param ctx CRC module context pointer
 * @return true on success
 */
bool uni_hal_crc_init(uni_hal_crc_context_t *ctx);

/**
 * Initializes CRC module
 * @param ctx CRC module context pointer
 * @return true on success
 */
bool uni_hal_crc_is_inited(const uni_hal_crc_context_t *ctx);


/**
 * Append new bytes to the CRC calculation
 * @param ctx CRC context
 * @param data data to append
 * @param data_len data length
 * @return true on success
 */
bool uni_hal_crc_append(uni_hal_crc_context_t *ctx, const uint8_t *data, uint32_t data_len);


/**
 * Perform calculation cycle (reset -> append -> get)
 * @param ctx CRC context
 * @param data dato to be processed
 * @param data_len data length
 * @return CRC value
 */
uint32_t uni_hal_crc_calculate(uni_hal_crc_context_t *ctx, const uint8_t *data, uint32_t data_len);


/**
 * Perform calculation cycle (reset -> append -> get) with custom initial value
 * @param ctx CRC context
 * @param initial_value initial value
 * @param data dato to be processed
 * @param data_len data length
 * @return CRC value
 */
uint32_t uni_hal_crc_calculate_ex(uni_hal_crc_context_t *ctx, uint32_t initial_value, const uint8_t *data, uint32_t data_len);


/**
 * Get current resulting CRC value
 * @param ctx CRC context
 * @return CRC value
 */
uint32_t uni_hal_crc_get_value(const uni_hal_crc_context_t *ctx);


/**
 * Reset CRC to the initial value
 * @param ctx CRC context
 * @return true on success
 */
bool uni_hal_crc_reset(uni_hal_crc_context_t *ctx);


/**
 * Reset CRC to the custom initial value
 * @param ctx CRC context
 * @param value initial value
 * @return true on success
 */
bool uni_hal_crc_reset_ex(uni_hal_crc_context_t *ctx, uint32_t value);

#if defined(__cplusplus)
}
#endif
