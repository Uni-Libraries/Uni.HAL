#pragma once

#include "adc/uni_hal_adc_typedef.h"

// The regular sequencer has 16 ranks on both supported STM32 families.
static inline bool uni_hal_adc_stm32_config_valid(const uni_hal_adc_config_t *config,
                                                   uint32_t last_channel,
                                                   uni_hal_dma_channel_e first_dma_channel) {
    if (config->instance < UNI_HAL_CORE_PERIPH_ADC_1 || config->instance > UNI_HAL_CORE_PERIPH_ADC_3 ||
        config->channels_count == 0U || config->channels_count > UNI_HAL_ADC_CHANNELS_MAX) {
        return false;
    }
    for (uint32_t rank = 0; rank < config->channels_count; ++rank) {
        if (config->channels[rank] > last_channel) {
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
