#pragma once

//
// Includes
//

// ST
#include <stm32l4xx.h>

// Uni.HAL
#include "core/uni_hal_core_enum.h"
#include "dma/uni_hal_dma_enum.h"



//
// Functions
//

/**
 * Get DMA module handle
 * @param module DMA module to retrieve
 * @return module handle
 */
DMA_TypeDef *uni_hal_dma_stm32l4_get_module(uni_hal_core_periph_e module);


/**
 * Get DMA channel register value
 * @param channel DMA channel to get
 * @return DMA channel reg value
 */
uint32_t uni_hal_dma_stm32l4_get_channel(uni_hal_dma_channel_e channel);

