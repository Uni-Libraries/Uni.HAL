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
#include "rcc/uni_hal_rcc_enum.h"


//
// Defines
//

#define UNI_HAL_ADC_CHANNELS_MAX (16U)



//
// Typedefs
//

/**
 * Where the ADC takes its conversion clock from
 */
typedef enum {
    /**
     * The kernel clock selected by `clock_source`, divided by `clock_divider`:
     * 1, 2, 4, 6, 8, 10, 12, 16, 32, 64, 128 or 256
     */
    UNI_HAL_ADC_CLOCK_ASYNC = 0,

    /**
     * The bus clock of the ADC, divided by `clock_divider`: 1, 2 or 4. A divider of 1 is
     * allowed only while the AHB prescaler is 1.
     */
    UNI_HAL_ADC_CLOCK_SYNC,
} uni_hal_adc_clock_e;


typedef struct {
    /**
     * ADC instance
     */
    uni_hal_core_periph_e instance;

    /**
     * ADC kernel clock source. On the STM32L4: SYSCLK, also when left at zero, or NONE.
     */
    uni_hal_rcc_clksrc_e clock_source;

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
     * Longest wait in ms for the calibration to finish, and again for the ADC to become ready.
     * Both need the ADC clock; an ADC without it makes uni_hal_adc_init() fail after this time.
     * 0: 500 ms.
     */
    uint32_t timeout;

    /**
     * Conversion clock. It is shared by the ADCs of one common block (ADC1..ADC3 on the STM32L4;
     * ADC1 with ADC2 on the STM32H7): the ADC that is initialised first decides, and the setting
     * of an ADC initialised while another one of the block is running is not applied.
     *
     * This and the three settings below decide how long a conversion takes. Their zero values
     * give what the driver has always used.
     */
    uni_hal_adc_clock_e clock_mode;

    /**
     * Divider of the conversion clock, see uni_hal_adc_clock_e.
     * 0: 1, except for the asynchronous clock of the STM32H7, where it is 8.
     * @note the driver leaves the BOOST setting of the STM32H7 at its reset value, which is
     *       meant for a slow ADC clock
     */
    uint32_t clock_divider;

    /**
     * Shortest acceptable sampling time in ADC clock cycles, for all channels. The driver takes
     * the shortest setting of the ADC that is at least this long; every setting lasts half a
     * cycle longer than its whole number: 2, 6, 12, 24, 47, 92, 247 and 640 on the STM32L4;
     * 1, 2, 8, 16, 32, 64, 387 and 810 on the STM32H7.
     * 0: the longest one.
     * @note the internal channels need several microseconds, see the datasheet
     */
    uint32_t sampling_cycles;

    /**
     * Resolution in bits: 6, 8, 10 or 12 on the STM32L4; 8, 10, 12, 14 or 16 on the STM32H7.
     * 0: 12 bits on the STM32L4, 16 on the STM32H7.
     * The raw values are right-aligned at this width; the functions that return millivolts or
     * a temperature take it into account.
     */
    uint32_t resolution_bits;

} uni_hal_adc_config_t;


typedef struct {
    bool initialized;

    /**
     * Resolution the ADC runs with, as the LL_ADC_RESOLUTION_xx value of the target
     */
    uint32_t resolution;

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
