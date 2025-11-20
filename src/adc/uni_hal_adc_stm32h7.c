//
// Includes
//

// stdlib
#include <stddef.h>

// st
#include <stm32h7xx_ll_adc.h>
#include <stm32h7xx_ll_bus.h>
#include <stm32h7xx_ll_dma.h>

// FreeRTOS
#include <FreeRTOS.h>

// Uni.Common
#include <uni_common.h>

// Uni.HAL
#include "adc/uni_hal_adc.h"
#include "rcc/uni_hal_rcc.h"



//
// Defines
//

#define UNI_HAL_ADC_VBAT_DIV (3.0f)
#define UNI_HAL_ADC_INT_PRIO (4U)
#define UNI_HAL_ADC_DELAY_STARTUP (1U)



//
// IRQ
//

void ADC1_2_IRQHandler(void) {
    traceISR_ENTER();

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

    portYIELD_FROM_ISR(pdFALSE);
}

void ADC3_IRQHandler(void) {
    traceISR_ENTER();

    if (LL_ADC_IsActiveFlag_EOC(ADC3)) {
        LL_ADC_ClearFlag_EOC(ADC3);
    }
    if (LL_ADC_IsActiveFlag_EOS(ADC3)) {
        LL_ADC_ClearFlag_EOS(ADC3);
    }
    if (LL_ADC_IsActiveFlag_OVR(ADC3)) {
        LL_ADC_ClearFlag_OVR(ADC3);
    }

    portYIELD_FROM_ISR(pdFALSE);
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
        result = LL_ADC_CHANNEL_19;
        break;
    case UNI_HAL_ADC_STM32H7_CHANNEL_REFINT:
        result = LL_ADC_CHANNEL_VREFINT;
        break;
    case UNI_HAL_ADC_STM32H7_CHANNEL_TEMPSENSOR:
        result = LL_ADC_CHANNEL_TEMPSENSOR;
        break;
    case UNI_HAL_ADC_STM32H7_CHANNEL_VBAT:
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
        result = ADC_IRQn;
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

    ADC_Common_TypeDef * common_instance = __LL_ADC_COMMON_INSTANCE(_uni_hal_adc_get_instance(ctx->config.instance));
    if (!__LL_ADC_IS_ENABLED_ALL_COMMON_INSTANCE(common_instance)) {
        // internal path
        uint32_t internal_path = LL_ADC_PATH_INTERNAL_NONE;
        for (uint32_t idx_channel = 0; idx_channel < ctx->config.channels_count; idx_channel++) {
            switch (ctx->config.channels[idx_channel]) {
            case UNI_HAL_ADC_STM32H7_CHANNEL_REFINT:
                internal_path |= LL_ADC_PATH_INTERNAL_VREFINT;
                break;
            case UNI_HAL_ADC_STM32H7_CHANNEL_TEMPSENSOR:
                internal_path |= LL_ADC_PATH_INTERNAL_TEMPSENSOR;
                break;
            case UNI_HAL_ADC_STM32H7_CHANNEL_VBAT:
                internal_path |= LL_ADC_PATH_INTERNAL_VBAT;
                break;
            default:
                break;
            }
        }

        LL_ADC_SetCommonClock(common_instance, LL_ADC_CLOCK_ASYNC_DIV8);
        LL_ADC_SetMultimode(common_instance, LL_ADC_MULTI_INDEPENDENT);
        LL_ADC_SetCommonPathInternalCh(common_instance, internal_path);

        result = true;
    }
    else{
        result = true;
    }


    return result;
}

bool _uni_hal_adc_configure_dma(uni_hal_adc_context_t *ctx){
    DMA_TypeDef* module = uni_hal_dma_stm32h7_get_module(ctx->config.dma->config.instance);
    uint32_t stream = uni_hal_dma_stm32h7_get_channel(ctx->config.dma->config.channel);

    uint32_t request = 0;
    switch(ctx->config.instance){
        case UNI_HAL_CORE_PERIPH_ADC_1:
            request = LL_DMAMUX1_REQ_ADC1;
            break;
        case UNI_HAL_CORE_PERIPH_ADC_2:
            request = LL_DMAMUX1_REQ_ADC2;
            break;
        case UNI_HAL_CORE_PERIPH_ADC_3:
            request = LL_DMAMUX1_REQ_ADC3;
            break;
        default:
            break;
    }
    LL_DMA_SetPeriphRequest(module, stream, request);

    LL_DMA_SetDataTransferDirection(module, stream, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
    LL_DMA_SetStreamPriorityLevel(module, stream, LL_DMA_PRIORITY_LOW);

    uni_hal_dma_set_priority(ctx->config.dma, UNI_HAL_DMA_PRIORITY_LOW);
    uni_hal_dma_set_mode(ctx->config.dma, UNI_HAL_DMA_MODE_CIRCULAR);

    LL_DMA_SetPeriphIncMode(module, stream, LL_DMA_PERIPH_NOINCREMENT);
    LL_DMA_SetMemoryIncMode(module, stream, LL_DMA_MEMORY_INCREMENT);
    LL_DMA_SetPeriphSize(module, stream, LL_DMA_PDATAALIGN_HALFWORD);
    LL_DMA_SetMemorySize(module, stream, LL_DMA_MDATAALIGN_HALFWORD);

    uni_hal_dma_set_fifo_mode(ctx->config.dma, false);

    LL_DMA_ConfigAddresses(module, stream, LL_ADC_DMA_GetRegAddr(_uni_hal_adc_get_instance(ctx->config.instance), LL_ADC_DMA_REG_REGULAR_DATA),
                           (uint32_t) ctx->config.data, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
    LL_DMA_SetDataLength(module, stream, ctx->config.channels_count);

    //LL_DMA_EnableIT_TC(module, stream);
    //LL_DMA_EnableIT_TE(module, stream);

    return true;
}

bool _uni_hal_adc_configure(uni_hal_adc_context_t *ctx) {
    bool result = false;
    ADC_TypeDef *instance = _uni_hal_adc_get_instance(ctx->config.instance);
    if (instance != NULL) {
        // module
        LL_ADC_InitTypeDef adc = {
                .Resolution = LL_ADC_RESOLUTION_16B,
                .LowPowerMode = LL_ADC_LP_MODE_NONE,
                .LeftBitShift = LL_ADC_LEFT_BIT_SHIFT_NONE,
        };
        LL_ADC_Init(instance, &adc);

        // reg
        LL_ADC_REG_InitTypeDef reg = {
                .TriggerSource = LL_ADC_REG_TRIG_SOFTWARE,
                .ContinuousMode = LL_ADC_REG_CONV_CONTINUOUS,
                .DataTransferMode =  ctx->config.dma != NULL ? LL_ADC_REG_DMA_TRANSFER_UNLIMITED : LL_ADC_REG_DR_TRANSFER,
                .SequencerLength = _uni_hal_adc_get_scan_length(ctx->config.channels_count),
                .SequencerDiscont = LL_ADC_REG_SEQ_DISCONT_DISABLE,
                .Overrun = LL_ADC_REG_OVR_DATA_OVERWRITTEN,
        };
        LL_ADC_REG_Init(instance, &reg);


        if(ctx->config.channels_count>0U) {
            LL_ADC_SetChannelPreSelection(instance, _uni_hal_adc_get_channel(ctx->config.channels[0]));
            // ranks
            for (uint32_t idx_channel = 0; idx_channel < ctx->config.channels_count; idx_channel++) {
                uint32_t channel = _uni_hal_adc_get_channel(ctx->config.channels[idx_channel]);
                LL_ADC_REG_SetSequencerRanks(instance, _uni_hal_adc_get_rank(idx_channel + 1), channel);
                LL_ADC_SetChannelSamplingTime(instance, channel, LL_ADC_SAMPLINGTIME_810CYCLES_5);
                LL_ADC_SetChannelSingleDiff(instance, channel, LL_ADC_SINGLE_ENDED);
            }
        }

        //LL_ADC_EnableIT_EOS(instance);
        //LL_ADC_EnableIT_OVR(instance);

        result = true;
    }

    return result;
}


bool _uni_hal_adc_powerup(uni_hal_adc_context_t *ctx) {
    bool result = false;
    ADC_TypeDef *instance = _uni_hal_adc_get_instance(ctx->config.instance);
    if (instance != NULL) {
        // Disable ADC deep power down (enabled by default after reset state)
        LL_ADC_DisableDeepPowerDown(instance);

        // Enable ADC internal voltage regulator
        LL_ADC_EnableInternalRegulator(instance);

        // Wait for regulator
        volatile uint32_t wait_loop_index = ((LL_ADC_DELAY_INTERNAL_REGUL_STAB_US * (SystemCoreClock / (100000 * 2))) / 10);
        while(wait_loop_index != 0) {
            wait_loop_index--;
        }

        // Start calibration
        LL_ADC_StartCalibration(instance, LL_ADC_CALIB_OFFSET_LINEARITY, LL_ADC_SINGLE_ENDED);

        // Wait for calibration completion
        while (LL_ADC_IsCalibrationOnGoing(instance)) {
        }

        result = true;
    }

    return result;
}

void _uni_hal_adc_enable(uni_hal_adc_context_t *ctx) {
    ADC_TypeDef *instance = _uni_hal_adc_get_instance(ctx->config.instance);
    LL_ADC_Enable(instance);
    while (!LL_ADC_IsActiveFlag_ADRDY(instance)) {
    }
}

bool _uni_hal_adc_trigger(uni_hal_adc_context_t *ctx) {
    bool result = false;
    ADC_TypeDef *instance = _uni_hal_adc_get_instance(ctx->config.instance);
    if (instance != NULL) {
        if (!LL_ADC_REG_IsConversionOngoing(instance)) {
            LL_ADC_REG_StartConversion(instance);
            result = true;
        }
    }

    return result;
}



//
// Functions
//

bool uni_hal_adc_init(uni_hal_adc_context_t *ctx) {
    bool result = false;

    if (ctx != NULL && ctx->config.channels_count > 0U) {
        result = true;

        // clock
        uint32_t adc_interrupt = _uni_hal_adc_get_interrupt(ctx->config.instance);

        // gpio
        for (size_t idx = 0; idx < ctx->config.channels_count; idx++) {
            if (ctx->config.pins[idx] != NULL && !uni_hal_gpio_pin_is_inited(ctx->config.pins[idx])) {
                ctx->config.pins[idx]->gpio_type = UNI_HAL_GPIO_TYPE_ANALOG;
                ctx->config.pins[idx]->gpio_pull = UNI_HAL_GPIO_PULL_NO;
                result = uni_hal_gpio_pin_init(ctx->config.pins[idx]) && result;
            }
        }

        // clk
        result = uni_hal_rcc_clksrc_set(ctx->config.instance, UNI_HAL_RCC_CLKSRC_PLL2P) && result;
        result = uni_hal_rcc_clk_set(ctx->config.instance, true) && result;

        // irq
        NVIC_SetPriority(adc_interrupt, UNI_HAL_ADC_INT_PRIO); /* ADC IRQ greater priority than DMA IRQ */
        NVIC_EnableIRQ(adc_interrupt);

        // dma
        if (ctx->config.dma) {
            uni_hal_dma_init(ctx->config.dma);
            result = _uni_hal_adc_configure_dma(ctx) && result;
            result = uni_hal_dma_enable(ctx->config.dma, true) && result;
        }

        // common
        result = _uni_hal_adc_configure_common(ctx) && result;

        // module
        result = _uni_hal_adc_configure(ctx) && result;

        // powerup
        result = _uni_hal_adc_powerup(ctx) && result;

        // enable
        ctx->state.initialized = result;
        if (result) {
            _uni_hal_adc_enable(ctx);
            _uni_hal_adc_trigger(ctx);
        }
    }

    return result;
}


uint16_t uni_hal_adc_get_channel_mv(const uni_hal_adc_context_t *ctx, uint32_t channel) {
    uint16_t result = UINT16_MAX;
    if (uni_hal_adc_is_inited(ctx)) {
        result = __LL_ADC_CALC_DATA_TO_VOLTAGE(ctx->config.v_ref, uni_hal_adc_get_channel_raw(ctx, channel), LL_ADC_RESOLUTION_16B);
    }

    return result;
}



//
// Functions/H7
//

static int32_t _uni_hal_adc_calc_temperature(uint32_t vref_analog_voltage, uint32_t tempsensor_adc_data, uint32_t adc_resolution) {
    int32_t temp_sensor_cal1 = (int32_t) *TEMPSENSOR_CAL1_ADDR;
    int32_t temp_sensor_cal2 = (int32_t) *TEMPSENSOR_CAL2_ADDR;
    int32_t temp_sensor_cal1_temp = (int32_t) TEMPSENSOR_CAL1_TEMP;
    int32_t temp_sensor_cal2_temp = (int32_t) TEMPSENSOR_CAL2_TEMP;
    int32_t temp_sensor_cal_vref_analog = (int32_t) TEMPSENSOR_CAL_VREFANALOG;

    int32_t temp_sensor_adc_data_converted = (int32_t) __LL_ADC_CONVERT_DATA_RESOLUTION(tempsensor_adc_data, adc_resolution, LL_ADC_RESOLUTION_16B);

    // Calculate temperature in millidegrees Celsius
    // Formula:
    // Temperature = ((TS_ADC_DATA - TS_CAL1) * (TS_CAL2_TEMP - TS_CAL1_TEMP)) / (TS_CAL2 - TS_CAL1) + TS_CAL1_TEMP
    //
    // To improve precision and return millidegrees:
    // 1. Scale TS_CAL1_TEMP and TS_CAL2_TEMP to millidegrees (multiply by 1000)
    // 2. Perform calculation
    
    int32_t temp_diff_deg_c = temp_sensor_cal2_temp - temp_sensor_cal1_temp;
    int32_t cal_diff = temp_sensor_cal2 - temp_sensor_cal1;

    // Scale voltage to avoid precision loss during division
    // We want to calculate: (ADC_DATA * VREF) / CAL_VREF
    // This gives us the ADC value as if it was measured at CAL_VREF (3.3V)
    int32_t adc_calibrated = (temp_sensor_adc_data_converted * (int32_t)vref_analog_voltage) / temp_sensor_cal_vref_analog;

    int32_t numerator = (adc_calibrated - temp_sensor_cal1) * temp_diff_deg_c * 1000;
    
    return (numerator / cal_diff) + (temp_sensor_cal1_temp * 1000);
}

int32_t uni_hal_adc_stm32h7_get_mcutemp(const uni_hal_adc_context_t *ctx) {
    int32_t result = 0;
    if (uni_hal_adc_is_inited(ctx) && ctx->config.instance == UNI_HAL_CORE_PERIPH_ADC_3) {
        result = _uni_hal_adc_calc_temperature(ctx->config.v_ref,
                                               uni_hal_adc_get_channel_raw(ctx, UNI_HAL_ADC_STM32H7_CHANNEL_TEMPSENSOR),
                                               LL_ADC_RESOLUTION_16B);
    }
    return result;
}

uint32_t uni_hal_adc_stm32h7_get_vdda(uni_hal_adc_context_t* ctx){
    uint32_t result = UINT32_MAX;
    if(uni_hal_adc_is_inited(ctx) && ctx->config.instance == UNI_HAL_CORE_PERIPH_ADC_3){
        result = 3300U * (*VREFINT_CAL_ADDR) / uni_hal_adc_get_channel_raw(ctx, UNI_HAL_ADC_STM32H7_CHANNEL_REFINT);
    }
    return result;
}
