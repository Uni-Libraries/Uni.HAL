//
// Includes
//

// stdlib
#include <stddef.h>

// STM
#include <stm32h7xx_ll_bus.h>
#include <stm32h7xx_ll_dma.h>

// FreeRTOS
#include <FreeRTOS.h>

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

void DMA1_Stream0_IRQHandler(void) {
    traceISR_ENTER();

    // transfer complete
    if (LL_DMA_IsActiveFlag_TC0(DMA1)) {
        LL_DMA_ClearFlag_TC0(DMA1);
    }

    // transfer error
    if (LL_DMA_IsActiveFlag_TE0(DMA1)) {
        LL_DMA_ClearFlag_TE0(DMA1);
    }

    portYIELD_FROM_ISR(pdFALSE);
}

void DMA1_Stream1_IRQHandler(void) {
    traceISR_ENTER();

    // transfer complete
    if (LL_DMA_IsActiveFlag_TC1(DMA1)) {
        LL_DMA_ClearFlag_TC1(DMA1);
    }

    // transfer error
    if (LL_DMA_IsActiveFlag_TE1(DMA1)) {
        LL_DMA_ClearFlag_TE1(DMA1);
    }

    portYIELD_FROM_ISR(pdFALSE);
}

void DMA1_Stream2_IRQHandler(void) {
    traceISR_ENTER();

    // transfer complete
    if (LL_DMA_IsActiveFlag_TC2(DMA1)) {
        LL_DMA_ClearFlag_TC2(DMA1);
    }

    // transfer error
    if (LL_DMA_IsActiveFlag_TE2(DMA1)) {
        LL_DMA_ClearFlag_TE2(DMA1);
    }

    portYIELD_FROM_ISR(pdFALSE);
}

void DMA1_Stream3_IRQHandler(void) {
    traceISR_ENTER();

    // transfer complete
    if (LL_DMA_IsActiveFlag_TC3(DMA1)) {
        LL_DMA_ClearFlag_TC3(DMA1);
    }

    // transfer error
    if (LL_DMA_IsActiveFlag_TE3(DMA1)) {
        LL_DMA_ClearFlag_TE3(DMA1);
    }

    portYIELD_FROM_ISR(pdFALSE);
}

// Streams without a dedicated handler above: uni_hal_dma_init() enables their
// interrupt as well, so they need a handler that acknowledges the flags.
#define UNI_HAL_DMA_IRQ_HANDLER(module, stream)                 \
    void module##_Stream##stream##_IRQHandler(void) {           \
        traceISR_ENTER();                                       \
                                                                \
        if (LL_DMA_IsActiveFlag_TC##stream(module)) {           \
            LL_DMA_ClearFlag_TC##stream(module);                \
        }                                                       \
                                                                \
        if (LL_DMA_IsActiveFlag_TE##stream(module)) {           \
            LL_DMA_ClearFlag_TE##stream(module);                \
        }                                                       \
                                                                \
        portYIELD_FROM_ISR(pdFALSE);                            \
    }

UNI_HAL_DMA_IRQ_HANDLER(DMA1, 4)
UNI_HAL_DMA_IRQ_HANDLER(DMA1, 5)
UNI_HAL_DMA_IRQ_HANDLER(DMA1, 6)
UNI_HAL_DMA_IRQ_HANDLER(DMA1, 7)
UNI_HAL_DMA_IRQ_HANDLER(DMA2, 0)
UNI_HAL_DMA_IRQ_HANDLER(DMA2, 1)
UNI_HAL_DMA_IRQ_HANDLER(DMA2, 2)
UNI_HAL_DMA_IRQ_HANDLER(DMA2, 3)
UNI_HAL_DMA_IRQ_HANDLER(DMA2, 4)
UNI_HAL_DMA_IRQ_HANDLER(DMA2, 5)
UNI_HAL_DMA_IRQ_HANDLER(DMA2, 6)
UNI_HAL_DMA_IRQ_HANDLER(DMA2, 7)


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
        case UNI_HAL_DMA_CHANNEL_0:
            result = LL_DMA_STREAM_0;
            break;
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
                case UNI_HAL_DMA_CHANNEL_0:
                    result = DMA1_Stream0_IRQn;
                break;
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
                case UNI_HAL_DMA_CHANNEL_0:
                    result = DMA2_Stream0_IRQn;
                break;
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

    // a stream that is already set up is not an error
    if (ctx != NULL && ctx->state.initialized) {
        return true;
    }

    if (ctx != NULL) {
        result = uni_hal_rcc_clk_set(ctx->config.instance, true);

        uint32_t interrupt = _uni_hal_dma_get_interrupt(ctx->config.instance, ctx->config.channel);
        NVIC_SetPriority(interrupt, UNI_HAL_DMA_INTERRUPT_PRIORITY);
        NVIC_EnableIRQ(interrupt);

        ctx->state.initialized = result;
    }

    return result;
}


bool uni_hal_dma_enable(uni_hal_dma_context_t *ctx, bool val) {
    bool result = false;
    if (uni_hal_dma_is_inited(ctx)) {
        DMA_TypeDef *module = uni_hal_dma_stm32h7_get_module(ctx->config.instance);
        uint32_t stream = uni_hal_dma_stm32h7_get_channel(ctx->config.channel);
        val != false ? LL_DMA_EnableStream(module, stream) : LL_DMA_DisableStream(module, stream);
        result = true;
    }
    return result;
}



//
// Functions/Setters
//

bool uni_hal_dma_set_fifo_mode(uni_hal_dma_context_t *ctx, bool val) {
    bool result = false;
    if (uni_hal_dma_is_inited(ctx)) {
        DMA_TypeDef *module = uni_hal_dma_stm32h7_get_module(ctx->config.instance);
        uint32_t stream = uni_hal_dma_stm32h7_get_channel(ctx->config.channel);
        val != false ? LL_DMA_EnableFifoMode(module, stream) : LL_DMA_DisableFifoMode(module, stream);
        result = true;
    }
    return result;
}


bool uni_hal_dma_set_mode(uni_hal_dma_context_t *ctx, uni_hal_dma_mode_e val) {
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


bool uni_hal_dma_set_priority(uni_hal_dma_context_t *ctx, uni_hal_dma_priority_e val) {
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
