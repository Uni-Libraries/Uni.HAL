//
// Includes
//

// stdlib
#include <math.h>
#include <stddef.h>

// STM LL
#include <stm32l496xx.h>
#include <stm32l4xx_ll_adc.h>
#include <stm32l4xx_ll_bus.h>
#include <stm32l4xx_ll_dma.h>

// Uni.Common
#include <uni_common.h>

// Uni.HAL
#include "adc/uni_hal_adc.h"
#include "adc/uni_hal_adc_stm32_config.h"
#include "core/uni_hal_core.h"
#include "dwt/uni_hal_dwt.h"
#include "gpio/uni_hal_gpio.h"
#include "rcc/uni_hal_rcc.h"
#include "adc/uni_hal_adc_stm32_startup.h"



//
// Defines
//

#define UNI_HAL_ADC_VBAT_DIV (3.0f)


#define UNI_HAL_ADC_INT_PRIO (1U)

#define UNI_HAL_ADC_DELAY_STARTUP (1U)




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


/**
 * Get the conversion clock setting of the configuration
 * @param config ADC configuration
 * @return LL_ADC_CLOCK_xx value, UINT32_MAX for a divider the clock mode does not have
 */
static uint32_t _uni_hal_adc_get_clock(const uni_hal_adc_config_t *config) {
    uint32_t result = UINT32_MAX;

    if (config->clock_mode == UNI_HAL_ADC_CLOCK_SYNC) {
        switch (config->clock_divider) {
        case 0U:
        case 1U:
            result = LL_ADC_CLOCK_SYNC_PCLK_DIV1;
            break;
        case 2U:
            result = LL_ADC_CLOCK_SYNC_PCLK_DIV2;
            break;
        case 4U:
            result = LL_ADC_CLOCK_SYNC_PCLK_DIV4;
            break;
        default:
            break;
        }
    }
    else if (config->clock_mode == UNI_HAL_ADC_CLOCK_ASYNC) {
        switch ((config->clock_divider != 0U) ? config->clock_divider : 1U) {
        case 1U:
            result = LL_ADC_CLOCK_ASYNC_DIV1;
            break;
        case 2U:
            result = LL_ADC_CLOCK_ASYNC_DIV2;
            break;
        case 4U:
            result = LL_ADC_CLOCK_ASYNC_DIV4;
            break;
        case 6U:
            result = LL_ADC_CLOCK_ASYNC_DIV6;
            break;
        case 8U:
            result = LL_ADC_CLOCK_ASYNC_DIV8;
            break;
        case 10U:
            result = LL_ADC_CLOCK_ASYNC_DIV10;
            break;
        case 12U:
            result = LL_ADC_CLOCK_ASYNC_DIV12;
            break;
        case 16U:
            result = LL_ADC_CLOCK_ASYNC_DIV16;
            break;
        case 32U:
            result = LL_ADC_CLOCK_ASYNC_DIV32;
            break;
        case 64U:
            result = LL_ADC_CLOCK_ASYNC_DIV64;
            break;
        case 128U:
            result = LL_ADC_CLOCK_ASYNC_DIV128;
            break;
        case 256U:
            result = LL_ADC_CLOCK_ASYNC_DIV256;
            break;
        default:
            break;
        }
    }
    else {
        // unknown mode
    }

    return result;
}


/**
 * Get the shortest sampling time that is at least as long as asked for
 * @param cycles shortest acceptable sampling time in ADC clock cycles, 0 for the longest
 * @return LL_ADC_SAMPLINGTIME_xx value, UINT32_MAX when the ADC has no setting that long
 */
static uint32_t _uni_hal_adc_get_sampling(uint32_t cycles) {
    uint32_t result = UINT32_MAX;

    if (cycles == 0U) {
        result = LL_ADC_SAMPLINGTIME_640CYCLES_5;
    }
    else if (cycles <= 2U) {
        result = LL_ADC_SAMPLINGTIME_2CYCLES_5;
    }
    else if (cycles <= 6U) {
        result = LL_ADC_SAMPLINGTIME_6CYCLES_5;
    }
    else if (cycles <= 12U) {
        result = LL_ADC_SAMPLINGTIME_12CYCLES_5;
    }
    else if (cycles <= 24U) {
        result = LL_ADC_SAMPLINGTIME_24CYCLES_5;
    }
    else if (cycles <= 47U) {
        result = LL_ADC_SAMPLINGTIME_47CYCLES_5;
    }
    else if (cycles <= 92U) {
        result = LL_ADC_SAMPLINGTIME_92CYCLES_5;
    }
    else if (cycles <= 247U) {
        result = LL_ADC_SAMPLINGTIME_247CYCLES_5;
    }
    else if (cycles <= 640U) {
        result = LL_ADC_SAMPLINGTIME_640CYCLES_5;
    }
    else {
        // longer than the ADC can sample
    }

    return result;
}


/**
 * Get the resolution setting
 * @param bits resolution in bits, 0 for the default of the driver
 * @return LL_ADC_RESOLUTION_xx value, UINT32_MAX for a width the ADC does not have
 */
static uint32_t _uni_hal_adc_get_resolution(uint32_t bits) {
    uint32_t result = UINT32_MAX;

    switch (bits) {
    case 0U:
        result = LL_ADC_RESOLUTION_12B;
        break;
    case 6U:
        result = LL_ADC_RESOLUTION_6B;
        break;
    case 8U:
        result = LL_ADC_RESOLUTION_8B;
        break;
    case 10U:
        result = LL_ADC_RESOLUTION_10B;
        break;
    case 12U:
        result = LL_ADC_RESOLUTION_12B;
        break;
    default:
        break;
    }

    return result;
}


bool _uni_hal_adc_configure_common(uni_hal_adc_context_t *ctx) {
    uint32_t const clock = _uni_hal_adc_get_clock(&ctx->config);
    if (clock == UINT32_MAX) {
        return false;
    }

    uint32_t paths = LL_ADC_PATH_INTERNAL_NONE;
    for (uint32_t i = 0U; i < ctx->config.channels_count; ++i) {
        switch (ctx->config.channels[i]) {
        case UNI_HAL_ADC_STM32L4_CHANNEL_REFINT:
            paths |= LL_ADC_PATH_INTERNAL_VREFINT;
            break;
        case UNI_HAL_ADC_STM32L4_CHANNEL_TEMPSENSOR:
            paths |= LL_ADC_PATH_INTERNAL_TEMPSENSOR;
            break;
        case UNI_HAL_ADC_STM32L4_CHANNEL_VBAT:
            paths |= LL_ADC_PATH_INTERNAL_VBAT;
            break;
        default:
            break;
        }
    }

    // All three ADCs share CCR. The clock and multimode require disabled ADCs.
    // Each new scan can enable its analog inputs independently (RM0351, ADC_CCR).
    uint32_t const mask = uni_hal_core_irq_pause();
    if (!__LL_ADC_IS_ENABLED_ALL_COMMON_INSTANCE(ADC123_COMMON)) {
        LL_ADC_SetCommonClock(ADC123_COMMON, clock);
        LL_ADC_SetMultimode(ADC123_COMMON, LL_ADC_MULTI_INDEPENDENT);
        LL_ADC_SetCommonPathInternalCh(ADC123_COMMON, paths);
    } else if (paths != LL_ADC_PATH_INTERNAL_NONE) {
        LL_ADC_SetCommonPathInternalChAdd(ADC123_COMMON, paths);
    }
    uni_hal_core_irq_resume(mask);
    return true;
}


bool _uni_hal_adc_configure(uni_hal_adc_context_t *ctx) {
    bool result = false;
    ADC_TypeDef *instance = _uni_hal_adc_get_instance(ctx->config.instance);
    uint32_t const resolution = _uni_hal_adc_get_resolution(ctx->config.resolution_bits);
    uint32_t const sampling = _uni_hal_adc_get_sampling(ctx->config.sampling_cycles);
    if (instance != NULL && resolution != UINT32_MAX && sampling != UINT32_MAX) {
        ctx->state.resolution = resolution;
        LL_ADC_SetResolution(instance, resolution);
        LL_ADC_SetDataAlignment(instance, LL_ADC_DATA_ALIGN_RIGHT);
        LL_ADC_SetLowPowerMode(instance, LL_ADC_LP_MODE_NONE);

        // sequence
        LL_ADC_REG_SetTrigSource(instance, LL_ADC_REG_TRIG_SOFTWARE);
        // the sequence is started once and repeats; DMA in circular mode keeps the data array current
        LL_ADC_REG_SetContinuousMode(instance, LL_ADC_REG_CONV_CONTINUOUS);
        LL_ADC_REG_SetDMATransfer(instance, ctx->config.dma ? LL_ADC_REG_DMA_TRANSFER_UNLIMITED : LL_ADC_REG_DMA_TRANSFER_NONE);
        LL_ADC_REG_SetOverrun(instance, LL_ADC_REG_OVR_DATA_OVERWRITTEN);
        LL_ADC_REG_SetSequencerLength(instance, _uni_hal_adc_get_scan_length(ctx->config.channels_count));

        // Set ranks
        for (uint32_t idx_channel = 0; idx_channel < ctx->config.channels_count; idx_channel++) {
            uint32_t channel = _uni_hal_adc_get_channel(ctx->config.channels[idx_channel]);
            LL_ADC_REG_SetSequencerRanks(instance, _uni_hal_adc_get_rank(idx_channel + 1), channel);
            LL_ADC_SetChannelSamplingTime(instance, channel, sampling);
            LL_ADC_SetChannelSingleDiff(instance, channel, LL_ADC_SINGLE_ENDED);
        }

        // no interrupt for the end of the sequence: it would come every few tens of microseconds
        LL_ADC_EnableIT_OVR(instance);

        result = true;
    }

    return result;
}


static bool _uni_hal_adc_dma_route_valid(const uni_hal_adc_config_t *config) {
    if (config->dma == nullptr) {
        return true;
    }
    // RM0351 tables 44 and 45: ADC1/2/3 feed DMA1 channels 1/2/3 and DMA2 channels 3/4/5.
    const uni_hal_dma_config_t *dma = &config->dma->config;
    unsigned const first_channel = dma->instance == UNI_HAL_CORE_PERIPH_DMA_1 ? UNI_HAL_DMA_CHANNEL_1 : UNI_HAL_DMA_CHANNEL_3;
    return dma->channel == first_channel + (config->instance - UNI_HAL_CORE_PERIPH_ADC_1);
}

// An initializer may yield during calibration. Reserve this converter until its
// configuration is complete, including for scans without DMA.
static bool initializing_adc[3];

static bool prepare_dma_channel(uni_hal_dma_context_t *dma) {
    if (!uni_hal_dma_init(dma)) {
        return false;
    }
    DMA_TypeDef *module = uni_hal_dma_stm32l4_get_module(dma->config.instance);
    uint32_t const channel = uni_hal_dma_stm32l4_get_channel(dma->config.channel);
    return !LL_DMA_IsEnabledChannel(module, channel);
}

static bool start_dma(uni_hal_adc_context_t *ctx) {
    DMA_TypeDef *module = uni_hal_dma_stm32l4_get_module(ctx->config.dma->config.instance);
    uint32_t const channel = uni_hal_dma_stm32l4_get_channel(ctx->config.dma->config.channel);
    uint32_t const mask = uni_hal_core_irq_pause();
    // Another DMA client could have started the channel during ADC calibration.
    if (LL_DMA_IsEnabledChannel(module, channel)) {
        uni_hal_core_irq_resume(mask);
        return false;
    }
    LL_DMA_SetPeriphRequest(module, channel, LL_DMA_REQUEST_0);
    LL_DMA_SetDataTransferDirection(module, channel, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
    LL_DMA_SetChannelPriorityLevel(module, channel, LL_DMA_PRIORITY_LOW);
    LL_DMA_SetMode(module, channel, LL_DMA_MODE_CIRCULAR);
    LL_DMA_SetPeriphIncMode(module, channel, LL_DMA_PERIPH_NOINCREMENT);
    LL_DMA_SetMemoryIncMode(module, channel, LL_DMA_MEMORY_INCREMENT);
    LL_DMA_SetPeriphSize(module, channel, LL_DMA_PDATAALIGN_HALFWORD);
    LL_DMA_SetMemorySize(module, channel, LL_DMA_MDATAALIGN_HALFWORD);
    LL_DMA_ConfigAddresses(module, channel, LL_ADC_DMA_GetRegAddr(_uni_hal_adc_get_instance(ctx->config.instance), LL_ADC_DMA_REG_REGULAR_DATA),
                           (uint32_t)ctx->config.data, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
    LL_DMA_SetDataLength(module, channel, ctx->config.channels_count);
    LL_DMA_EnableIT_TE(module, channel);
    bool const started = uni_hal_dma_enable(ctx->config.dma, true);
    uni_hal_core_irq_resume(mask);
    return started;
}

bool _uni_hal_adc_powerup(uni_hal_adc_context_t *ctx) {
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

        if (!uni_hal_adc_wait_startup_flag(&instance->CR, ADC_CR_ADCAL, false, ctx->config.timeout)) {
            return false;
        }
        uni_hal_dwt_delay_ms(1U);

        result = true;
    }

    return result;
}


bool _uni_hal_adc_enable(uni_hal_adc_context_t *ctx) {
    ADC_TypeDef *instance = _uni_hal_adc_get_instance(ctx->config.instance);
    LL_ADC_Enable(instance);

    return uni_hal_adc_wait_startup_flag(&instance->ISR, ADC_ISR_ADRDY, true, ctx->config.timeout);
}


static bool _uni_hal_adc_trigger(uni_hal_adc_context_t *ctx) {
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


//
// Functions
//

static bool initialize_converter(uni_hal_adc_context_t *ctx, uni_hal_rcc_clksrc_e clock_source) {
    const uni_hal_adc_config_t *config = &ctx->config;
    for (uint32_t rank = 0; rank < config->channels_count; ++rank) {
        uni_hal_gpio_pin_context_t *pin = config->pins[rank];
        if (pin != nullptr && !uni_hal_gpio_pin_is_inited(pin)) {
            if (!uni_hal_gpio_pin_init(pin)) {
                return false;
            }
        }
    }

    if (!uni_hal_rcc_clksrc_set(config->instance, clock_source) || !uni_hal_rcc_clk_set(config->instance, true)) {
        return false;
    }
    const uint32_t interrupt = _uni_hal_adc_get_interrupt(config->instance);
    NVIC_SetPriority(interrupt, UNI_HAL_ADC_INT_PRIO);
    NVIC_EnableIRQ(interrupt);

    if (!_uni_hal_adc_configure_common(ctx) || !_uni_hal_adc_configure(ctx) ||
        !_uni_hal_adc_powerup(ctx) || !_uni_hal_adc_enable(ctx)) {
        return false;
    }
    if (config->dma != nullptr && !start_dma(ctx)) {
        // No conversion has been started. Return the ADC to a configurable state for a retry.
        ADC_TypeDef *adc = _uni_hal_adc_get_instance(config->instance);
        LL_ADC_Disable(adc);
        if (uni_hal_adc_wait_startup_flag(&adc->CR, ADC_CR_ADEN | ADC_CR_ADDIS, false, config->timeout)) {
            LL_ADC_ClearFlag_ADRDY(adc);
        }
        return false;
    }
    uint32_t const mask = uni_hal_core_irq_pause();
    ctx->state.initialized = true;
    _uni_hal_adc_trigger(ctx);
    uni_hal_core_irq_resume(mask);
    return true;
}

bool uni_hal_adc_init(uni_hal_adc_context_t *ctx) {
    if (ctx == nullptr) {
        return false;
    }
    const uni_hal_adc_config_t *config = &ctx->config;
    if (!uni_hal_adc_stm32_config_valid(config, UNI_HAL_DMA_CHANNEL_1) ||
        !_uni_hal_adc_dma_route_valid(config) || _uni_hal_adc_get_clock(config) == UINT32_MAX ||
        _uni_hal_adc_get_resolution(config->resolution_bits) == UINT32_MAX ||
        _uni_hal_adc_get_sampling(config->sampling_cycles) == UINT32_MAX) {
        return false;
    }
    uni_hal_rcc_clksrc_e clock_source = config->clock_source;
    if (clock_source == UNI_HAL_RCC_CLKSRC_UNKNOWN) {
        clock_source = UNI_HAL_RCC_CLKSRC_SYSCLK;
    }
    if (clock_source != UNI_HAL_RCC_CLKSRC_SYSCLK && clock_source != UNI_HAL_RCC_CLKSRC_NONE) {
        return false;
    }
    unsigned const adc = config->instance - UNI_HAL_CORE_PERIPH_ADC_1;
    uint32_t mask = uni_hal_core_irq_pause();
    if (ctx->state.initialized || initializing_adc[adc]) {
        bool const ready = uni_hal_adc_is_inited(ctx);
        uni_hal_core_irq_resume(mask);
        return ready;
    }
    if (config->dma && !prepare_dma_channel(config->dma)) {
        uni_hal_core_irq_resume(mask);
        return false;
    }
    initializing_adc[adc] = true;
    uni_hal_core_irq_resume(mask);

    bool const ready = initialize_converter(ctx, clock_source);
    mask = uni_hal_core_irq_pause();
    initializing_adc[adc] = false;
    uni_hal_core_irq_resume(mask);
    return ready;
}


uint16_t uni_hal_adc_get_channel_mv(const uni_hal_adc_context_t *ctx, uint32_t channel) {
    uint16_t const raw = uni_hal_adc_get_channel_raw(ctx, channel);
    if (raw == UINT16_MAX) {
        return UINT16_MAX;
    }
    return __LL_ADC_CALC_DATA_TO_VOLTAGE(ctx->config.v_ref, raw, ctx->state.resolution);
}


float uni_hal_adc_get_channel_voltage(const uni_hal_adc_context_t *ctx, uint32_t channel) {
    uint16_t const mv = uni_hal_adc_get_channel_mv(ctx, channel);
    return mv == UINT16_MAX ? NAN : mv / 1000.0f;
}


//
// Functions/MCUTEMP
//

uint16_t uni_hal_adc_mcutemp_raw(const uni_hal_adc_context_t *ctx) {
    return uni_hal_adc_get_channel_raw(ctx, UNI_HAL_ADC_STM32L4_CHANNEL_TEMPSENSOR);
}

float uni_hal_adc_mcutemp_get(const uni_hal_adc_context_t *ctx) {
    uint16_t const raw = uni_hal_adc_mcutemp_raw(ctx);
    return raw == UINT16_MAX ? NAN : __LL_ADC_CALC_TEMPERATURE(ctx->config.v_ref, raw, ctx->state.resolution);
}



//
// Functions/L4
//

uint32_t uni_hal_adc_stm32l4_get_vdda(const uni_hal_adc_context_t *ctx) {
    uint16_t const raw = uni_hal_adc_get_channel_raw(ctx, UNI_HAL_ADC_STM32L4_CHANNEL_REFINT);
    if (raw == 0U || raw == UINT16_MAX) {
        return UINT32_MAX;
    }
    return __LL_ADC_CALC_VREFANALOG_VOLTAGE(raw, ctx->state.resolution);
}


int32_t uni_hal_adc_stm32l4_get_mcutemp(const uni_hal_adc_context_t *ctx) {
    int32_t result = INT32_MAX;
    uint16_t const sample = uni_hal_adc_get_channel_raw(ctx, UNI_HAL_ADC_STM32L4_CHANNEL_TEMPSENSOR);
    if (sample != UINT16_MAX) {
        // the supply voltage scales the reading: take the measured one when there is one
        uint32_t vdda = uni_hal_adc_stm32l4_get_vdda(ctx);
        if (vdda == UINT32_MAX) {
            vdda = ctx->config.v_ref;
        }

        // the calibration values were taken with 12 bits
        int32_t const raw = (int32_t)__LL_ADC_CONVERT_DATA_RESOLUTION(
                (uint32_t)sample,
                ctx->state.resolution, LL_ADC_RESOLUTION_12B);
        int32_t const cal_1 = (int32_t)(*TEMPSENSOR_CAL1_ADDR);
        int32_t const cal_2 = (int32_t)(*TEMPSENSOR_CAL2_ADDR);
        int32_t const cal_vdda = (int32_t)TEMPSENSOR_CAL_VREFANALOG;

        if (cal_2 != cal_1) {
            // the reading as it would be at the supply voltage of the calibration
            int32_t const raw_cal = ((raw * (int32_t)vdda) + (cal_vdda / 2)) / cal_vdda;
            result = ((int32_t)TEMPSENSOR_CAL1_TEMP * 1000) +
                     (((raw_cal - cal_1) * ((int32_t)TEMPSENSOR_CAL2_TEMP - (int32_t)TEMPSENSOR_CAL1_TEMP) * 1000) /
                      (cal_2 - cal_1));
        }
    }
    return result;
}
