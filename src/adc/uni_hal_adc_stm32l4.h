#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//
// Includes
//

// stdlib
#include <stdint.h>

// uni_hal
#include "adc/uni_hal_adc_typedef.h"



//
// Defines
//

// Numbers of the internal channels in the `channels` array of the configuration
#define UNI_HAL_ADC_STM32L4_CHANNEL_REFINT (19U)
#define UNI_HAL_ADC_STM32L4_CHANNEL_TEMPSENSOR (20U)
#define UNI_HAL_ADC_STM32L4_CHANNEL_VBAT (21U)



//
// Functions
//

/**
 * Get the temperature of the MCU from its internal sensor. The reading is corrected for the
 * supply voltage: the measured one when VREFINT is among the channels of this ADC, `v_ref` of
 * the configuration otherwise.
 * @param ctx pointer to the ADC context, with the temperature sensor among its channels
 * @return temperature in millidegrees Celsius, INT32_MAX when it cannot be determined
 */
int32_t uni_hal_adc_stm32l4_get_mcutemp(const uni_hal_adc_context_t *ctx);

/**
 * Get the analog supply voltage, measured against the internal reference
 * @param ctx pointer to the ADC context, with VREFINT among its channels
 * @return VDDA in millivolts, UINT32_MAX when it cannot be determined
 */
uint32_t uni_hal_adc_stm32l4_get_vdda(const uni_hal_adc_context_t *ctx);

#if defined(__cplusplus)
}
#endif
