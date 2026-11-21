#pragma once

#include "uni_hal_target.h"
#include "adc/uni_hal_adc.h"

static inline bool uni_hal_adc_stm32_channel_valid(uni_hal_core_periph_e instance, uint32_t channel) {
#if defined(UNI_HAL_TARGET_STM32H7)
    return channel < UNI_HAL_ADC_STM32H7_CHANNEL_REFINT ||
           (instance == UNI_HAL_CORE_PERIPH_ADC_3 && channel <= UNI_HAL_ADC_STM32H7_CHANNEL_VBAT);
#elif defined(UNI_HAL_TARGET_MCU_STM32L496)
    if (channel == UNI_HAL_ADC_STM32L4_CHANNEL_REFINT) {
        return instance == UNI_HAL_CORE_PERIPH_ADC_1;
    }
    return channel < UNI_HAL_ADC_STM32L4_CHANNEL_REFINT ||
           (instance != UNI_HAL_CORE_PERIPH_ADC_2 && channel <= UNI_HAL_ADC_STM32L4_CHANNEL_VBAT);
#endif
}

// The regular sequencer has 16 ranks on both supported STM32 families.
static inline bool uni_hal_adc_stm32_config_valid(const uni_hal_adc_config_t *config,
                                                   uni_hal_dma_channel_e first_dma_channel) {
    if (config->instance < UNI_HAL_CORE_PERIPH_ADC_1 || config->instance > UNI_HAL_CORE_PERIPH_ADC_3 ||
        config->channels_count == 0U || config->channels_count > UNI_HAL_ADC_CHANNELS_MAX) {
        return false;
    }
    for (uint32_t rank = 0; rank < config->channels_count; ++rank) {
        if (!uni_hal_adc_stm32_channel_valid(config->instance, config->channels[rank])) {
            return false;
        }
    }
    if (config->dma == nullptr) {
        return true;
    }
    const uni_hal_dma_config_t *dma = &config->dma->config;
    return config->data != nullptr && (uintptr_t)config->data % alignof(uint16_t) == 0U &&
           (dma->instance == UNI_HAL_CORE_PERIPH_DMA_1 || dma->instance == UNI_HAL_CORE_PERIPH_DMA_2) &&
           dma->channel >= first_dma_channel && dma->channel <= UNI_HAL_DMA_CHANNEL_7;
}
