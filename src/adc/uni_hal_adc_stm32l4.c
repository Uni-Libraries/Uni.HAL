//
// Includes
//

// stdlib
#include <stddef.h>

// STM LL
#include <stm32l496xx.h>
#include <stm32l4xx_ll_adc.h>
#include <stm32l4xx_ll_bus.h>

// Uni.Common
#include <uni_common.h>

// Uni.HAL
#include "adc/uni_hal_adc.h"
#include "dwt/uni_hal_dwt.h"
#include "gpio/uni_hal_gpio.h"
#include "rcc/uni_hal_rcc.h"


//
// Defines
//

#define UNI_HAL_ADC_VBAT_DIV (3.0f)

#define UNI_HAL_ADC_CHANNEL_REFINT (19U)
#define UNI_HAL_ADC_CHANNEL_TEMPSENSOR (20U)
#define UNI_HAL_ADC_CHANNEL_VBAT (21U)

#define UNI_HAL_ADC_INT_PRIO (1U)

#define UNI_HAL_ADC_DELAY_STARTUP (1U)


//
// Global
//

static volatile uint16_t _uni_hal_adc_data_1[UNI_HAL_ADC_CHANNELS_MAX] = {0};
static volatile uint16_t _uni_hal_adc_data_2[UNI_HAL_ADC_CHANNELS_MAX] = {0};
static volatile uint16_t _uni_hal_adc_data_3[UNI_HAL_ADC_CHANNELS_MAX] = {0};


//
// IRQ
//

void ADC1_2_IRQHandler(void) {
    if (LL_ADC_IsActiveFlag_EOC(ADC1)) {
        LL_ADC_ClearFlag_EOC(ADC1);
    }
    if (LL_ADC_IsActiveFlag_EOS(ADC1)) {
        LL_ADC_ClearFlag_EOS(ADC1);
    }
    if (LL_ADC_IsActiveFlag_OVR(ADC1)) {
        LL_ADC_ClearFlag_OVR(ADC1);
    }

    // ADC2
    if (LL_ADC_IsActiveFlag_EOC(ADC2)) {
        LL_ADC_ClearFlag_EOC(ADC2);
    }
    if (LL_ADC_IsActiveFlag_EOS(ADC2)) {
        LL_ADC_ClearFlag_EOS(ADC2);
    }
    if (LL_ADC_IsActiveFlag_OVR(ADC2)) {
        LL_ADC_ClearFlag_OVR(ADC2);
    }
}

void ADC3_IRQHandler(void) {
    if (LL_ADC_IsActiveFlag_EOC(ADC3)) {
        LL_ADC_ClearFlag_EOC(ADC3);
    }
    if (LL_ADC_IsActiveFlag_EOS(ADC3)) {
        LL_ADC_ClearFlag_EOS(ADC3);
    }
    if (LL_ADC_IsActiveFlag_OVR(ADC3)) {
        LL_ADC_ClearFlag_OVR(ADC3);
    }
}


//
// PRIVATE
//

/**
 * Get channel bits from the channel index
 * @param channel_num index of channel
 * @return channel bits to use it LL_ADC_* functions
 */
static uint32_t _uni_hal_adc_get_channel(uint32_t channel_num) {
    uint32_t result = 0;
    switch (channel_num) {
    case 0:
        result = LL_ADC_CHANNEL_0;
        break;
    case 1:
        result = LL_ADC_CHANNEL_1;
        break;
    case 2:
        result = LL_ADC_CHANNEL_2;
        break;
    case 3:
        result = LL_ADC_CHANNEL_3;
        break;
    case 4:
        result = LL_ADC_CHANNEL_4;
        break;
    case 5:
        result = LL_ADC_CHANNEL_5;
        break;
    case 6:
        result = LL_ADC_CHANNEL_6;
        break;
    case 7:
        result = LL_ADC_CHANNEL_7;
        break;
    case 8:
        result = LL_ADC_CHANNEL_8;
        break;
    case 9:
        result = LL_ADC_CHANNEL_9;
        break;
    case 10:
        result = LL_ADC_CHANNEL_10;
        break;
    case 11:
        result = LL_ADC_CHANNEL_11;
        break;
    case 12:
        result = LL_ADC_CHANNEL_12;
        break;
    case 13:
        result = LL_ADC_CHANNEL_13;
        break;
    case 14:
        result = LL_ADC_CHANNEL_14;
        break;
    case 15:
        result = LL_ADC_CHANNEL_15;
        break;
    case 16:
        result = LL_ADC_CHANNEL_16;
        break;
    case 17:
        result = LL_ADC_CHANNEL_17;
        break;
    case 18:
        result = LL_ADC_CHANNEL_18;
        break;
    case 19:
        result = LL_ADC_CHANNEL_VREFINT;
        break;
    case 20:
        result = LL_ADC_CHANNEL_TEMPSENSOR;
        break;
    case 21:
        result = LL_ADC_CHANNEL_VBAT;
        break;
    default:
        break;
    }

    return result;
}

uint32_t _uni_hal_adc_get_interrupt(uni_hal_core_periph_e instance) {
    uint32_t result = 0U;
    switch (instance) {
    case UNI_HAL_CORE_PERIPH_ADC_1:
    case UNI_HAL_CORE_PERIPH_ADC_2:
        result = ADC1_2_IRQn;
        break;
    case UNI_HAL_CORE_PERIPH_ADC_3:
        result = ADC3_IRQn;
        break;
    default:
        break;
    }

    return result;
}


/**
 * Get scan length register value according to the needed sequencer length
 * @param length sequencer length
 * @return register value
 */
static uint32_t _uni_hal_adc_get_scan_length(uint32_t length) {
    uint32_t result = LL_ADC_REG_SEQ_SCAN_DISABLE;
    switch (length) {
    case 1:
        result = LL_ADC_REG_SEQ_SCAN_DISABLE;
        break;
    case 2:
        result = LL_ADC_REG_SEQ_SCAN_ENABLE_2RANKS;
        break;
    case 3:
        result = LL_ADC_REG_SEQ_SCAN_ENABLE_3RANKS;
        break;
    case 4:
        result = LL_ADC_REG_SEQ_SCAN_ENABLE_4RANKS;
        break;
    case 5:
        result = LL_ADC_REG_SEQ_SCAN_ENABLE_5RANKS;
        break;
    case 6:
        result = LL_ADC_REG_SEQ_SCAN_ENABLE_6RANKS;
        break;
    case 7:
        result = LL_ADC_REG_SEQ_SCAN_ENABLE_7RANKS;
        break;
    case 8:
        result = LL_ADC_REG_SEQ_SCAN_ENABLE_8RANKS;
        break;
    case 9:
        result = LL_ADC_REG_SEQ_SCAN_ENABLE_9RANKS;
        break;
    case 10:
        result = LL_ADC_REG_SEQ_SCAN_ENABLE_10RANKS;
        break;
    case 11:
        result = LL_ADC_REG_SEQ_SCAN_ENABLE_11RANKS;
        break;
    case 12:
        result = LL_ADC_REG_SEQ_SCAN_ENABLE_12RANKS;
        break;
    case 13:
        result = LL_ADC_REG_SEQ_SCAN_ENABLE_13RANKS;
        break;
    case 14:
        result = LL_ADC_REG_SEQ_SCAN_ENABLE_14RANKS;
        break;
    case 15:
        result = LL_ADC_REG_SEQ_SCAN_ENABLE_15RANKS;
        break;
    case 16:
        result = LL_ADC_REG_SEQ_SCAN_ENABLE_16RANKS;
        break;
    default:
        break;
    }

    return result;
}

/**
 * Get rank register value according to rank index
 * @param rank_idx rank index
 * @return rank register value
 */
static uint32_t _uni_hal_adc_get_rank(uint32_t rank_idx) {
    uint32_t result = 0U;
    switch (rank_idx) {
    case 1:
        result = LL_ADC_REG_RANK_1;
        break;
    case 2:
        result = LL_ADC_REG_RANK_2;
        break;
    case 3:
        result = LL_ADC_REG_RANK_3;
        break;
    case 4:
        result = LL_ADC_REG_RANK_4;
        break;
    case 5:
        result = LL_ADC_REG_RANK_5;
        break;
    case 6:
        result = LL_ADC_REG_RANK_6;
        break;
    case 7:
        result = LL_ADC_REG_RANK_7;
        break;
    case 8:
        result = LL_ADC_REG_RANK_8;
        break;
    case 9:
        result = LL_ADC_REG_RANK_9;
        break;
    case 10:
        result = LL_ADC_REG_RANK_10;
        break;
    case 11:
        result = LL_ADC_REG_RANK_11;
        break;
    case 12:
        result = LL_ADC_REG_RANK_12;
        break;
    case 13:
        result = LL_ADC_REG_RANK_13;
        break;
    case 14:
        result = LL_ADC_REG_RANK_14;
        break;
    case 15:
        result = LL_ADC_REG_RANK_15;
        break;
    case 16:
        result = LL_ADC_REG_RANK_16;
        break;
    default:
        break;
    }

    return result;
}


/**
 * Get pointer to the ADC data array
 * @param instance ADC instance
 * @return pointer to the start of data array
 */
static volatile uint16_t *_uni_hal_adc_get_data(uni_hal_core_periph_e instance) {
    volatile uint16_t *result = NULL;

    switch (instance) {
    case UNI_HAL_CORE_PERIPH_ADC_1:
        result = _uni_hal_adc_data_1;
        break;
    case UNI_HAL_CORE_PERIPH_ADC_2:
        result = _uni_hal_adc_data_2;
        break;
    case UNI_HAL_CORE_PERIPH_ADC_3:
        result = _uni_hal_adc_data_3;
        break;
    default:
        break;
    }

    return result;
}


/**
 * Get ADC instance handle from instance enum
 * @param instance ADC instance
 * @return pointer to ADC handle
 */
static ADC_TypeDef *_uni_hal_adc_get_instance(uni_hal_core_periph_e instance) {
    ADC_TypeDef *result = NULL;

    switch (instance) {
    case UNI_HAL_CORE_PERIPH_ADC_1:
        result = ADC1;
        break;
    case UNI_HAL_CORE_PERIPH_ADC_2:
        result = ADC2;
        break;
    case UNI_HAL_CORE_PERIPH_ADC_3:
        result = ADC3;
        break;
    default:
        break;
    }

    return result;
}


bool _uni_hal_adc_configure_common(uni_hal_adc_context_t *ctx) {
    bool result = false;

    if (!__LL_ADC_IS_ENABLED_ALL_COMMON_INSTANCE(instance)) {
        // internal path
        uint32_t internal_path = LL_ADC_PATH_INTERNAL_NONE;
        for (uint32_t idx_channel = 0; idx_channel < ctx->config.channels_count; idx_channel++) {
            switch (ctx->config.channels[idx_channel]) {
            case UNI_HAL_ADC_CHANNEL_REFINT:
                internal_path |= LL_ADC_PATH_INTERNAL_VREFINT;
                break;
            case UNI_HAL_ADC_CHANNEL_TEMPSENSOR:
                internal_path |= LL_ADC_PATH_INTERNAL_TEMPSENSOR;
                break;
            case UNI_HAL_ADC_CHANNEL_VBAT:
                internal_path |= LL_ADC_PATH_INTERNAL_VBAT;
                break;
            default:
                break;
            }
        }

        LL_ADC_SetCommonClock(__LL_ADC_COMMON_INSTANCE(instance), LL_ADC_CLOCK_ASYNC_DIV1);
        LL_ADC_SetMultimode(__LL_ADC_COMMON_INSTANCE(instance), LL_ADC_MULTI_INDEPENDENT);
        LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(adc_instance), internal_path);

        result = true;
    }
    else{
        result = true;
    }


    return result;
}


bool _uni_hal_adc_configure(uni_hal_adc_context_t *ctx) {
    bool result = false;
    ADC_TypeDef *instance = _uni_hal_adc_get_instance(ctx->config.instance);
    if (instance != NULL) {
        LL_ADC_SetResolution(instance, LL_ADC_RESOLUTION_12B);
        LL_ADC_SetDataAlignment(instance, LL_ADC_DATA_ALIGN_RIGHT);
        LL_ADC_SetLowPowerMode(instance, LL_ADC_LP_MODE_NONE);

        // sequence
        LL_ADC_REG_SetTrigSource(instance, LL_ADC_REG_TRIG_SOFTWARE);
        LL_ADC_REG_SetContinuousMode(instance, LL_ADC_REG_CONV_SINGLE);
        LL_ADC_REG_SetDMATransfer(instance, ctx->config.dma ? LL_ADC_REG_DMA_TRANSFER_UNLIMITED : LL_ADC_REG_DMA_TRANSFER_NONE);
        LL_ADC_REG_SetOverrun(instance, LL_ADC_REG_OVR_DATA_OVERWRITTEN);
        LL_ADC_REG_SetSequencerLength(instance, _uni_hal_adc_get_scan_length(ctx->config.channels_count));

        // Set ranks
        for (uint32_t idx_channel = 0; idx_channel < ctx->config.channels_count; idx_channel++) {
            uint32_t channel = _uni_hal_adc_get_channel(ctx->config.channels[idx_channel]);
            LL_ADC_REG_SetSequencerRanks(instance, _uni_hal_adc_get_rank(idx_channel + 1), channel);
            LL_ADC_SetChannelSamplingTime(instance, channel, LL_ADC_SAMPLINGTIME_640CYCLES_5);
            LL_ADC_SetChannelSingleDiff(instance, channel, LL_ADC_SINGLE_ENDED);
        }

        LL_ADC_EnableIT_EOS(instance);
        LL_ADC_EnableIT_OVR(instance);

        result = true;
    }

    return result;
}


bool _uni_hal_adc_start(uni_hal_adc_context_t *ctx) {
    bool result = false;
    ADC_TypeDef *instance = _uni_hal_adc_get_instance(ctx->config.instance);
    if (instance != NULL) {
        // Disable ADC deep power down (enabled by default after reset state)
        LL_ADC_DisableDeepPowerDown(instance);

        // Enable ADC internal voltage regulator
        LL_ADC_EnableInternalRegulator(instance);

        // Delay for ADC internal voltage regulator stabilization.
        uni_hal_dwt_delay_ms(1U);

        // Start calibration
        LL_ADC_StartCalibration(instance, true);

        // Wait for calibration completion
        while (LL_ADC_IsCalibrationOnGoing(instance)) {
        }
        uni_hal_dwt_delay_ms(1U);

        // Enable ADC
        LL_ADC_Enable(instance);
        while (!LL_ADC_IsActiveFlag_ADRDY(instance)) {
        }

        result = true;
    }

    return result;
}


//
// Functions
//

bool uni_hal_adc_init(uni_hal_adc_context_t *ctx) {
    bool result = false;

    if (ctx != NULL && ctx->config.channels_count > 0U) {
        ADC_TypeDef *adc_instance = _uni_hal_adc_get_instance(ctx->config.instance);
        uint32_t adc_interrupt = _uni_hal_adc_get_interrupt(ctx->config.instance);

        result = adc_instance != NULL;

        // configure DMA
        if (ctx->config.dma) {
            result = uni_hal_dma_init(ctx->config.dma) && result;
            //TODO: , LL_ADC_DMA_GetRegAddr(adc_instance, LL_ADC_DMA_REG_REGULAR_DATA), (uint32_t) _uni_hal_adc_get_data(ctx->config.instance), ctx->config.channels_count
        }

        // configure pins
        for (size_t idx = 0; idx < ctx->config.channels_count; idx++) {
            if (ctx->config.pins[idx] != NULL && !uni_hal_gpio_pin_is_inited(ctx->config.pins[idx])) {
                result = uni_hal_gpio_pin_init(ctx->config.pins[idx]) && result;
            }
        }

        // enable clock
        result = uni_hal_rcc_clk_set(ctx->config.instance, true) && result;

        // enable interrupts
        NVIC_SetPriority(adc_interrupt, UNI_HAL_ADC_INT_PRIO); /* ADC IRQ greater priority than DMA IRQ */
        NVIC_EnableIRQ(adc_interrupt);

        result = _uni_hal_adc_configure_common(ctx) && result;
        result = _uni_hal_adc_configure(ctx) && result;
        result = _uni_hal_adc_start(ctx) && result;
        ctx->state.initialized = result;

        if (result) {
            //TODO uni_hal_adc_trigger(ctx);
        }
    }

    return result;
}


bool uni_hal_adc_trigger(uni_hal_adc_context_t *ctx) {
    bool result = false;
    if (uni_hal_adc_is_inited(ctx)) {
        ADC_TypeDef *instance = _uni_hal_adc_get_instance(ctx->config.instance);
        if (instance != NULL) {
            if (!LL_ADC_REG_IsConversionOngoing(instance)) {
                LL_ADC_REG_StartConversion(instance);
                result = true;
            }
        }
    }

    return result;
}

uint16_t uni_hal_adc_get_rank_raw(const uni_hal_adc_context_t *ctx, uint32_t rank) {
    uint16_t result = 0U;

    if (uni_hal_adc_is_inited(ctx) && rank < ctx->config.channels_count) {
        const volatile uint16_t *array = _uni_hal_adc_get_data(ctx->config.instance);
        if (array != NULL) {
            result = array[rank];
        }
    }

    return result;
}


uint16_t uni_hal_adc_get_channel_raw(const uni_hal_adc_context_t *ctx, uint32_t channel_idx) {
    uint16_t result = 0U;

    if (uni_hal_adc_is_inited(ctx)) {
        for (size_t rank_idx = 0; rank_idx < uni_common_math_min(ctx->config.channels_count, UNI_HAL_ADC_CHANNELS_MAX);
             rank_idx++) {
            if (ctx->config.channels[rank_idx] == channel_idx) {
                const volatile uint16_t *array = _uni_hal_adc_get_data(ctx->config.instance);
                if (array != NULL) {
                    result = array[rank_idx];
                    break;
                }
            }
        }
    }

    return result;
}

uint16_t uni_hal_adc_get_channel_mv(const uni_hal_adc_context_t *ctx, uint32_t channel) {
    uint32_t result = 0U;
    if (uni_hal_adc_is_inited(ctx)) {
        result = __LL_ADC_CALC_DATA_TO_VOLTAGE(ctx->config.v_ref, uni_hal_adc_get_channel_raw(ctx, channel), LL_ADC_RESOLUTION_12B);
    }

    return result;
}

float uni_hal_adc_get_channel_voltage(const uni_hal_adc_context_t *ctx, uint32_t channel) {
    return uni_hal_adc_get_channel_mv(ctx, channel) / 1000.0f;
}


//
// Functions/MCUTEMP
//

uint16_t uni_hal_adc_mcutemp_raw(const uni_hal_adc_context_t *ctx) {
    return uni_hal_adc_get_channel_raw(ctx, UNI_HAL_ADC_CHANNEL_TEMPSENSOR);
}

float uni_hal_adc_mcutemp_get(const uni_hal_adc_context_t *ctx) {
    return __LL_ADC_CALC_TEMPERATURE(ctx->config.v_ref, uni_hal_adc_mcutemp_raw(ctx), LL_ADC_RESOLUTION_12B);
}
