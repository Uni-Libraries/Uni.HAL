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
#include "adc/uni_hal_adc_typedef.h"
#include "core/uni_hal_core_enum.h"
#include "dma/uni_hal_dma.h"
#include "gpio/uni_hal_gpio.h"
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
    #include "adc/uni_hal_adc_stm32h7.h"
#endif




//
// Functions
//

/**
 * Initializes ADC module
 * @param ctx pointer to the ADC context
 * @return true on success
 */
bool uni_hal_adc_init(uni_hal_adc_context_t *ctx);

/**
 * Checks that ADC was properly initialized
 * @param ctx pointer to the ADC context
 * @return true on success
 */
bool uni_hal_adc_is_inited(const uni_hal_adc_context_t* ctx);

/**
 * Get raw ADC rank value
 * @param ctx ADC context
 * @param channel ADC channel
 * @return raw value in ADC points
 */
uint16_t uni_hal_adc_get_rank_raw(const uni_hal_adc_context_t *ctx, uint32_t rank);

/**
 * Get raw ADC value
 * @param ctx ADC context
 * @param channel ADC channel
 * @return raw value in ADC points
 */
uint16_t uni_hal_adc_get_channel_raw(const uni_hal_adc_context_t *ctx, uint32_t channel);

/**
 * Get ADC value in millivolts
 * @param ctx ADC context
 * @param channel ADC channel
 * @return ADC value in millivolts
 */
uint16_t uni_hal_adc_get_channel_mv(const uni_hal_adc_context_t *ctx, uint32_t channel);

#if defined(__cplusplus)
}
#endif
