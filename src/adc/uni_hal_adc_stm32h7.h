#pragma once

//
// Includes
//

// uni_hal
#include "adc/uni_hal_adc_typedef.h"



//
// Defines
//

#define UNI_HAL_ADC_STM32H7_CHANNEL_REFINT (20U)
#define UNI_HAL_ADC_STM32H7_CHANNEL_TEMPSENSOR (21U)
#define UNI_HAL_ADC_STM32H7_CHANNEL_VBAT (22U)



//
// Functions
//

/**
 * Receive current MCU temperature
 * @param ctx pointer to the ADC context
 * @return current MCU temp in degree of celsius
 */
int32_t uni_hal_adc_stm32h7_get_mcutemp(const uni_hal_adc_context_t *ctx);


/**
 * Recalculate VDDA according to VREFINT
 * @param ctx pointer to the ADC context
 * @return VDDA voltage in millivolts
 * @note must be called only on ADC3 with enabled measurement of REFINT
 */
uint32_t uni_hal_adc_stm32h7_get_vdda(uni_hal_adc_context_t* ctx);
