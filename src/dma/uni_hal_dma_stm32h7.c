//
// Includes
//

// stdlib
#include <stddef.h>

// STM
#include <stm32h7xx_ll_bus.h>
#include <stm32h7xx_ll_dma.h>

// uni_hal
#include "dma/uni_hal_dma.h"
#include "rcc/uni_hal_rcc.h"



//
// Defines
//

#define UNI_HAL_DMA_INTERRUPT_PRIORITY (4U)


//
// ISRs
//

void DMA1_Stream1_IRQHandler(void) {
    // transfer complete
    if (LL_DMA_IsActiveFlag_TC1(DMA1)) {
        LL_DMA_ClearFlag_TC1(DMA1);
    }

    // transfer error
    if (LL_DMA_IsActiveFlag_TE1(DMA1)) {
        LL_DMA_ClearFlag_TE1(DMA1);
    }
}

void DMA1_Stream2_IRQHandler(void) {
    // transfer complete
    if (LL_DMA_IsActiveFlag_TC2(DMA1)) {
        LL_DMA_ClearFlag_TC2(DMA1);
    }

    // transfer error
    if (LL_DMA_IsActiveFlag_TE3(DMA1)) {
        LL_DMA_ClearFlag_TE3(DMA1);
    }
}

void DMA1_Stream3_IRQHandler(void) {
    /*
    LL_DMA_DisableStream(DMA1, LL_DMA_STREAM_3);
    LL_DMA_DisableIT_TC(DMA1, LL_DMA_STREAM_3);
    LL_DMA_DisableIT_TE(DMA1, LL_DMA_STREAM_3);
*/
    // transfer complete
    if (LL_DMA_IsActiveFlag_TC3(DMA1)) {
        LL_DMA_ClearFlag_TC3(DMA1);
    }

    // transfer error
    if (LL_DMA_IsActiveFlag_TE3(DMA1)) {
        LL_DMA_ClearFlag_TE3(DMA1);
    }
}



//
// Private
//




//
// H7-specific
//

DMA_TypeDef *uni_hal_dma_stm32h7_get_module(uni_hal_core_periph_e module) {
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


uint32_t uni_hal_dma_stm32h7_get_channel(uni_hal_dma_channel_e channel) {
    uint32_t result = 0U;
    switch (channel) {
    case UNI_HAL_DMA_CHANNEL_1:
        result = LL_DMA_STREAM_1;
        break;
    case UNI_HAL_DMA_CHANNEL_2:
        result = LL_DMA_STREAM_2;
        break;
    case UNI_HAL_DMA_CHANNEL_3:
        result = LL_DMA_STREAM_3;
        break;
    case UNI_HAL_DMA_CHANNEL_4:
        result = LL_DMA_STREAM_4;
        break;
    case UNI_HAL_DMA_CHANNEL_5:
        result = LL_DMA_STREAM_5;
        break;
    case UNI_HAL_DMA_CHANNEL_6:
        result = LL_DMA_STREAM_6;
        break;
    case UNI_HAL_DMA_CHANNEL_7:
        result = LL_DMA_STREAM_7;
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
            result = DMA1_Stream1_IRQn;
            break;
        case UNI_HAL_DMA_CHANNEL_2:
            result = DMA1_Stream2_IRQn;
            break;
        case UNI_HAL_DMA_CHANNEL_3:
            result = DMA1_Stream3_IRQn;
            break;
        case UNI_HAL_DMA_CHANNEL_4:
            result = DMA1_Stream4_IRQn;
            break;
        case UNI_HAL_DMA_CHANNEL_5:
            result = DMA1_Stream5_IRQn;
            break;
        case UNI_HAL_DMA_CHANNEL_6:
            result = DMA1_Stream6_IRQn;
            break;
        case UNI_HAL_DMA_CHANNEL_7:
            result = DMA1_Stream7_IRQn;
            break;
        default:
            break;
        }
        break;
    case UNI_HAL_CORE_PERIPH_DMA_2:
        switch (channel) {
        case UNI_HAL_DMA_CHANNEL_1:
            result = DMA2_Stream1_IRQn;
            break;
        case UNI_HAL_DMA_CHANNEL_2:
            result = DMA2_Stream2_IRQn;
            break;
        case UNI_HAL_DMA_CHANNEL_3:
            result = DMA2_Stream3_IRQn;
            break;
        case UNI_HAL_DMA_CHANNEL_4:
            result = DMA2_Stream4_IRQn;
            break;
        case UNI_HAL_DMA_CHANNEL_5:
            result = DMA2_Stream5_IRQn;
            break;
        case UNI_HAL_DMA_CHANNEL_6:
            result = DMA2_Stream6_IRQn;
            break;
        case UNI_HAL_DMA_CHANNEL_7:
            result = DMA2_Stream7_IRQn;
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



//
// Public
//

bool uni_hal_dma_init(uni_hal_dma_context_t *ctx) {
    bool result = false;
    if (ctx != nullptr && ctx->state.initialized != true) {
        result = uni_hal_rcc_clk_set(ctx->config.instance, true);

        uint32_t interrupt = _uni_hal_dma_get_interrupt(ctx->config.instance, ctx->config.channel);
        NVIC_SetPriority(interrupt, UNI_HAL_DMA_INTERRUPT_PRIORITY);
        NVIC_EnableIRQ(interrupt);

        ctx->state.initialized = result;
    }

    return result;
}



bool uni_hal_dma_enable(uni_hal_dma_context_t* ctx, bool val) {
    bool result = false;
    if (uni_hal_dma_is_inited(ctx)) {
        DMA_TypeDef *module = uni_hal_dma_stm32h7_get_module(ctx->config.instance);
        uint32_t stream = uni_hal_dma_stm32h7_get_channel(ctx->config.channel);
        val != false ? LL_DMA_EnableStream(module, stream) : LL_DMA_DisableStream(module, stream);
        result = true;
    }
    return result;
}



/**
* GARBAGE
 *
 *      // Parse configuration
        DMA_TypeDef *dma_module = _uni_hal_dma_get_module(ctx->instance);
        uint32_t dma_interrupt = _uni_hal_dma_get_interrupt(ctx->instance, ctx->channel);
        uint32_t dma_channel = _uni_hal_dma_get_channel(ctx->channel);
        uint32_t dma_request = _uni_hal_dma_get_request(ctx->request);
        uint32_t dma_direction = _uni_hal_dma_get_direction(ctx->direction);


        // Enable clock
        if (result) {
            ctx->state.initialized = true;
        }
 *   // Enable interrupt
 *


            // configure
            LL_DMA_SetPeriphRequest(dma_module, dma_channel, dma_request);
            LL_DMA_SetDataTransferDirection(dma_module, dma_channel, dma_direction);
            LL_DMA_SetStreamPriorityLevel(dma_module, dma_channel, LL_DMA_PRIORITY_LOW);
            LL_DMA_SetMode(dma_module, dma_channel, LL_DMA_MODE_CIRCULAR);
            LL_DMA_SetPeriphIncMode(dma_module, dma_channel, LL_DMA_PERIPH_NOINCREMENT);
            LL_DMA_SetMemoryIncMode(dma_module, dma_channel, LL_DMA_MEMORY_INCREMENT);
            LL_DMA_SetPeriphSize(dma_module, dma_channel, LL_DMA_PDATAALIGN_HALFWORD);
            LL_DMA_SetMemorySize(dma_module, dma_channel, LL_DMA_MDATAALIGN_HALFWORD);

            LL_DMA_ConfigAddresses(dma_module, dma_channel, from, to, dma_direction);
            LL_DMA_SetDataLength(dma_module, dma_channel, length);

            LL_DMA_EnableIT_TC(dma_module, dma_channel);
            LL_DMA_EnableIT_TE(dma_module, dma_channel);

*/

//
// Functions/Setters
//

bool uni_hal_dma_set_fifo_mode(uni_hal_dma_context_t* ctx, bool val) {
    bool result = false;
    if (uni_hal_dma_is_inited(ctx)) {
        DMA_TypeDef *module = uni_hal_dma_stm32h7_get_module(ctx->config.instance);
        uint32_t stream = uni_hal_dma_stm32h7_get_channel(ctx->config.channel);
        val != false ? LL_DMA_EnableFifoMode(module, stream) : LL_DMA_DisableFifoMode(module, stream);
        result = true;
    }
    return result;
}


bool uni_hal_dma_set_mode(uni_hal_dma_context_t* ctx, uni_hal_dma_mode_e val) {
    bool result = false;
    if (uni_hal_dma_is_inited(ctx)) {
        DMA_TypeDef *module = uni_hal_dma_stm32h7_get_module(ctx->config.instance);
        uint32_t stream = uni_hal_dma_stm32h7_get_channel(ctx->config.channel);

        uint32_t mode;
        switch (val) {
            case UNI_HAL_DMA_MODE_CIRCULAR:
                mode = LL_DMA_MODE_CIRCULAR;
                break;
            case UNI_HAL_DMA_MODE_PERCTRL:
                mode = LL_DMA_MODE_PFCTRL;
                break;
            case UNI_HAL_DMA_MODE_NORMAL:
            default:
                mode = LL_DMA_MODE_NORMAL;
                break;
        }
        LL_DMA_SetMode(module, stream, mode);

        result = true;
    }

    return result;
}


bool uni_hal_dma_set_priority(uni_hal_dma_context_t* ctx, uni_hal_dma_priority_e val){
    bool result = false;
    if (uni_hal_dma_is_inited(ctx)) {
        DMA_TypeDef *module = uni_hal_dma_stm32h7_get_module(ctx->config.instance);
        uint32_t stream = uni_hal_dma_stm32h7_get_channel(ctx->config.channel);

        uint32_t priority;
        switch (val) {
            case UNI_HAL_DMA_PRIORITY_MEDIUM:
                priority = LL_DMA_PRIORITY_MEDIUM;
                break;
            case UNI_HAL_DMA_PRIORITY_HIGH:
                priority = LL_DMA_PRIORITY_HIGH;
                break;
            case UNI_HAL_DMA_PRIORITY_VERYHIGH:
                priority = LL_DMA_PRIORITY_VERYHIGH;
                break;
            case UNI_HAL_DMA_PRIORITY_LOW:
            default:
                priority = LL_DMA_PRIORITY_LOW;
                break;
        }
        LL_DMA_SetStreamPriorityLevel(module, stream, priority);
        result = true;
    }

    return result;
}
