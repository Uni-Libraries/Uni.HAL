#pragma once

//
// Includes
//

// stdlib
#include <stdbool.h>
#include <stdint.h>

// uni_hal
#include "core/uni_hal_core_enum.h"
#include "dma/uni_hal_dma.h"
#include "gpio/uni_hal_gpio.h"



//
// Defines
//

#define UNI_HAL_ADC_CHANNELS_MAX (16U)



//
// Typedefs
//


typedef struct {
    /**
     * ADC instance
     */
    uni_hal_core_periph_e instance;

    /**
     * DMA context
     */
    uni_hal_dma_context_t* dma;

    /**
     * ADC enabled channels
     * @note must be sorted
     */
    uint32_t channels[UNI_HAL_ADC_CHANNELS_MAX];

    /**
     * GPIO pins for channels
     * @note must be synced with ::channels and ::channels_count
     */
    uni_hal_gpio_pin_context_t * pins[UNI_HAL_ADC_CHANNELS_MAX];

    /**
     * Pointer to the data array
     */
    volatile uint16_t* data;

    /**
     * Number of enabled channels, must be synced with ::channels
     */
    uint32_t channels_count;

    /**
     * Reference voltage in milliVolts
     */
    uint32_t v_ref;

    /**
     * Timeout for ADC calibration and ADC startup
     */
    uint32_t timeout;

} uni_hal_adc_config_t;


typedef struct {
    bool initialized;

    struct {
        bool valid;

        int32_t tempsensor_1_val;
        int32_t tempsensor_1_temp;
        int32_t tempsensor_2_val;
        int32_t tempsensor_2_temp;
        int32_t tempsensor_vref_analog;
        uint16_t vref_int;
    } cal;

} uni_hal_adc_state_t;


/**
 * ADC context structure
 */
typedef struct {
    uni_hal_adc_config_t config;
    uni_hal_adc_state_t state;
} uni_hal_adc_context_t;
