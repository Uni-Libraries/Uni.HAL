//
// Includes
//

// stdlib
#include <stddef.h>

// STM
#include <stm32l496xx.h>
#include <stm32l4xx_ll_bus.h>
#include <stm32l4xx_ll_dma.h>

// Uni.HAL
#include "dma/uni_hal_dma.h"
#include "core/uni_hal_core.h"
#include "rcc/uni_hal_rcc.h"

//
// Defines
//

#define UNI_HAL_DMA_INTERRUPT_PRIORITY (2U)


//
// ISRs
//

void DMA1_Channel1_IRQHandler(void) {
    // transfer complete
    if (LL_DMA_IsActiveFlag_TC1(DMA1)) {
        LL_DMA_ClearFlag_GI1(DMA1);
    }

    // transfer error
    if (LL_DMA_IsActiveFlag_TE1(DMA1)) {
        LL_DMA_ClearFlag_TE1(DMA1);
    }
}

void DMA1_Channel2_IRQHandler(void) {}

void DMA1_Channel3_IRQHandler(void) {
    // transfer complete
    if (LL_DMA_IsActiveFlag_TC3(DMA1)) {
        LL_DMA_ClearFlag_GI3(DMA1);
    }

    // transfer error
    if (LL_DMA_IsActiveFlag_TE3(DMA1)) {
        LL_DMA_ClearFlag_TE3(DMA1);
    }
}

void DMA1_Channel4_IRQHandler(void) {}

void DMA1_Channel5_IRQHandler(void) {}

void DMA1_Channel6_IRQHandler(void) {}

void DMA1_Channel7_IRQHandler(void) {}

void DMA2_Channel1_IRQHandler(void) {}

void DMA2_Channel2_IRQHandler(void) {}

void DMA2_Channel3_IRQHandler(void) {}

void DMA2_Channel4_IRQHandler(void) {}

void DMA2_Channel5_IRQHandler(void) {}


//
// Private
//

/**
 * Get DMA module handle
 * @param module DMA module to retrieve
 * @return module handle
 */
DMA_TypeDef *_uni_hal_dma_get_module(uni_hal_core_periph_e module) {
    DMA_TypeDef *result = NULL;
    switch (module) {
    case UNI_HAL_CORE_PERIPH_DMA_1:
        result = DMA1;
        break;
    case UNI_HAL_CORE_PERIPH_DMA_2:
        result = DMA2;
        break;
    default:
        break;
    }

    return result;
}


/**
 * Get DMA channel register value
 * @param channel DMA channel to get
 * @return DMA channel reg value
 */
uint32_t _uni_hal_dma_get_channel(uni_hal_dma_channel_e channel) {
    uint32_t result = 0U;
    switch (channel) {
    case UNI_HAL_DMA_CHANNEL_1:
        result = LL_DMA_CHANNEL_1;
        break;
    case UNI_HAL_DMA_CHANNEL_2:
        result = LL_DMA_CHANNEL_2;
        break;
    case UNI_HAL_DMA_CHANNEL_3:
        result = LL_DMA_CHANNEL_3;
        break;
    case UNI_HAL_DMA_CHANNEL_4:
        result = LL_DMA_CHANNEL_4;
        break;
    case UNI_HAL_DMA_CHANNEL_5:
        result = LL_DMA_CHANNEL_5;
        break;
    case UNI_HAL_DMA_CHANNEL_6:
        result = LL_DMA_CHANNEL_6;
        break;
    case UNI_HAL_DMA_CHANNEL_7:
        result = LL_DMA_CHANNEL_7;
        break;
    default:
        break;
    }

    return result;
}


/**
 * Get DMA direction register value
 * @param channel DMA direction to get
 * @return DMA direction reg value
 */
uint32_t _uni_hal_dma_get_direction(uni_hal_dma_direction_e direction) {
    uint32_t result = 0U;
    switch (direction) {
    case UNI_HAL_DMA_DIRECTION_P2M:
        result = LL_DMA_DIRECTION_PERIPH_TO_MEMORY;
        break;
    case UNI_HAL_DMA_DIRECTION_M2P:
        result = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
        break;
    case UNI_HAL_DMA_DIRECTION_M2M:
        result = LL_DMA_DIRECTION_MEMORY_TO_MEMORY;
        break;
    default:
        break;
    }

    return result;
}


/**
 * Get DMA interrupt number
 * @param module DMA module
 * @param channel DMA channel
 * @return DMA interrupt number
 */
uint32_t _uni_hal_dma_get_interrupt(uni_hal_core_periph_e module, uni_hal_dma_channel_e channel) {
    uint32_t result = 0U;
    switch (module) {
    case UNI_HAL_CORE_PERIPH_DMA_1:
        switch (channel) {
        case UNI_HAL_DMA_CHANNEL_1:
            result = DMA1_Channel1_IRQn;
            break;
        case UNI_HAL_DMA_CHANNEL_2:
            result = DMA1_Channel2_IRQn;
            break;
        case UNI_HAL_DMA_CHANNEL_3:
            result = DMA1_Channel3_IRQn;
            break;
        case UNI_HAL_DMA_CHANNEL_4:
            result = DMA1_Channel4_IRQn;
            break;
        case UNI_HAL_DMA_CHANNEL_5:
            result = DMA1_Channel5_IRQn;
            break;
        case UNI_HAL_DMA_CHANNEL_6:
            result = DMA1_Channel6_IRQn;
            break;
        case UNI_HAL_DMA_CHANNEL_7:
            result = DMA1_Channel7_IRQn;
            break;
        default:
            break;
        }
        break;
    case UNI_HAL_CORE_PERIPH_DMA_2:
        switch (channel) {
        case UNI_HAL_DMA_CHANNEL_1:
            result = DMA2_Channel1_IRQn;
            break;
        case UNI_HAL_DMA_CHANNEL_2:
            result = DMA2_Channel2_IRQn;
            break;
        case UNI_HAL_DMA_CHANNEL_3:
            result = DMA2_Channel3_IRQn;
            break;
        case UNI_HAL_DMA_CHANNEL_4:
            result = DMA2_Channel4_IRQn;
            break;
        case UNI_HAL_DMA_CHANNEL_5:
            result = DMA2_Channel5_IRQn;
            break;
        case UNI_HAL_DMA_CHANNEL_6:
            result = DMA2_Channel6_IRQn;
            break;
        case UNI_HAL_DMA_CHANNEL_7:
            result = DMA2_Channel7_IRQn;
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }

    return result;
}


/**
 * Get DMA request reg value
 * @param request DMA request
 * @return register value
 */
uint32_t _uni_hal_dma_get_request(uni_hal_dma_request_e request) {
    uint32_t result = 0U;

    switch (request) {
    case UNI_HAL_DMA_REQUEST_0:
        result = LL_DMA_REQUEST_0;
        break;
    case UNI_HAL_DMA_REQUEST_1:
        result = LL_DMA_REQUEST_1;
        break;
    case UNI_HAL_DMA_REQUEST_2:
        result = LL_DMA_REQUEST_2;
        break;
    case UNI_HAL_DMA_REQUEST_3:
        result = LL_DMA_REQUEST_3;
        break;
    case UNI_HAL_DMA_REQUEST_4:
        result = LL_DMA_REQUEST_4;
        break;
    case UNI_HAL_DMA_REQUEST_5:
        result = LL_DMA_REQUEST_5;
        break;
    case UNI_HAL_DMA_REQUEST_6:
        result = LL_DMA_REQUEST_6;
        break;
    case UNI_HAL_DMA_REQUEST_7:
        result = LL_DMA_REQUEST_7;
        break;
    default:
        break;
    }

    return result;
}


//
// Public
//

bool uni_hal_dma_init(uni_hal_dma_context_t *ctx) {
    bool result = false;
    if (ctx != NULL) {
        // Parse configuration
        DMA_TypeDef *dma_module = _uni_hal_dma_get_module(ctx->config.instance);
        uint32_t dma_interrupt = _uni_hal_dma_get_interrupt(ctx->config.instance, ctx->config.channel);
        uint32_t dma_channel = _uni_hal_dma_get_channel(ctx->config.channel);
        //TODO uint32_t dma_request = _uni_hal_dma_get_request(ctx->config.request);
        //TODO uint32_t dma_direction = _uni_hal_dma_get_direction(ctx->config.direction);


        // Enable clock
        result = uni_hal_rcc_clk_set(ctx->config.instance, true);
        if (result) {
            // Enable interrupt
            NVIC_SetPriority(dma_interrupt, UNI_HAL_DMA_INTERRUPT_PRIORITY);
            NVIC_EnableIRQ(dma_interrupt);

            // configure
            //TODO LL_DMA_SetPeriphRequest(dma_module, dma_channel, dma_request);
            //TODO LL_DMA_SetDataTransferDirection(dma_module, dma_channel, dma_direction);
            LL_DMA_SetChannelPriorityLevel(dma_module, dma_channel, LL_DMA_PRIORITY_LOW);
            LL_DMA_SetMode(dma_module, dma_channel, LL_DMA_MODE_CIRCULAR);
            LL_DMA_SetPeriphIncMode(dma_module, dma_channel, LL_DMA_PERIPH_NOINCREMENT);
            LL_DMA_SetMemoryIncMode(dma_module, dma_channel, LL_DMA_MEMORY_INCREMENT);
            LL_DMA_SetPeriphSize(dma_module, dma_channel, LL_DMA_PDATAALIGN_HALFWORD);
            LL_DMA_SetMemorySize(dma_module, dma_channel, LL_DMA_MDATAALIGN_HALFWORD);

            //TODO LL_DMA_ConfigAddresses(dma_module, dma_channel, from, to, dma_direction);
            //TODO LL_DMA_SetDataLength(dma_module, dma_channel, length);

            LL_DMA_EnableIT_TC(dma_module, dma_channel);
            LL_DMA_EnableIT_TE(dma_module, dma_channel);

            LL_DMA_EnableChannel(dma_module, dma_channel);
            ctx->state.initialized = true;
        }
    }

    return result;
}
