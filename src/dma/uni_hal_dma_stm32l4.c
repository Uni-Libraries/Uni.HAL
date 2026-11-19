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

#define UNI_HAL_DMA_INTERRUPT_PRIORITY (4U)



//
// ISRs
//

// State belongs to the physical channel. A failed transfer stays visible after its W1C acknowledgement.
static volatile bool transfer_error[2][7];

static void handle_channel_irq(DMA_TypeDef *module, uint32_t channel)
{
    uint32_t const shift = 4U * channel;
    uint32_t const causes = module->ISR & ((DMA_ISR_TEIF1 | DMA_ISR_HTIF1 | DMA_ISR_TCIF1) << shift);
    if (causes & (DMA_ISR_TEIF1 << shift)) {
        transfer_error[module == DMA2][channel] = true;
        LL_DMA_DisableChannel(module, channel);
    }
    if (causes) {
        WRITE_REG(module->IFCR, causes);
    }
}

#define DMA_CHANNEL_IRQ(controller, number)                        \
    void controller##_Channel##number##_IRQHandler(void)           \
    {                                                              \
        handle_channel_irq(controller, LL_DMA_CHANNEL_##number);   \
    }

DMA_CHANNEL_IRQ(DMA1, 1)
DMA_CHANNEL_IRQ(DMA1, 2)
DMA_CHANNEL_IRQ(DMA1, 3)
DMA_CHANNEL_IRQ(DMA1, 4)
DMA_CHANNEL_IRQ(DMA1, 5)
DMA_CHANNEL_IRQ(DMA1, 6)
DMA_CHANNEL_IRQ(DMA1, 7)
DMA_CHANNEL_IRQ(DMA2, 1)
DMA_CHANNEL_IRQ(DMA2, 2)
DMA_CHANNEL_IRQ(DMA2, 3)
DMA_CHANNEL_IRQ(DMA2, 4)
DMA_CHANNEL_IRQ(DMA2, 5)
DMA_CHANNEL_IRQ(DMA2, 6)
DMA_CHANNEL_IRQ(DMA2, 7)


//
// Private
//

/**
 * Get DMA module handle
 * @param module DMA module to retrieve
 * @return module handle
 */
DMA_TypeDef *uni_hal_dma_stm32l4_get_module(uni_hal_core_periph_e module) {
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
uint32_t uni_hal_dma_stm32l4_get_channel(uni_hal_dma_channel_e channel) {
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
    if (ctx == NULL) {
        return false;
    }
    uint32_t const interrupt = _uni_hal_dma_get_interrupt(ctx->config.instance, ctx->config.channel);
    if (interrupt == 0U) {
        return false;
    }
    if (ctx->state.initialized) {
        return true;
    }
    if (!uni_hal_rcc_clk_set(ctx->config.instance, true)) {
        return false;
    }
    NVIC_SetPriority(interrupt, UNI_HAL_DMA_INTERRUPT_PRIORITY);
    NVIC_EnableIRQ(interrupt);
    ctx->state.initialized = true;
    return true;
}


bool uni_hal_dma_stm32l4_has_transfer_error(const uni_hal_dma_context_t *ctx)
{
    if (!uni_hal_dma_is_inited(ctx) || ctx->config.channel < UNI_HAL_DMA_CHANNEL_1 ||
        ctx->config.channel > UNI_HAL_DMA_CHANNEL_7) {
        return false;
    }
    DMA_TypeDef *module = uni_hal_dma_stm32l4_get_module(ctx->config.instance);
    uint32_t const channel = uni_hal_dma_stm32l4_get_channel(ctx->config.channel);
    return module != NULL && transfer_error[module == DMA2][channel];
}


bool uni_hal_dma_enable(uni_hal_dma_context_t *ctx, bool val) {
    if (!uni_hal_dma_is_inited(ctx)) {
        return false;
    }
    DMA_TypeDef *module = uni_hal_dma_stm32l4_get_module(ctx->config.instance);
    uint32_t const channel = uni_hal_dma_stm32l4_get_channel(ctx->config.channel);
    if (val) {
        uint32_t const primask = uni_hal_core_irq_pause();
        if (!LL_DMA_IsEnabledChannel(module, channel)) {
            // Clear stale events before starting, so a pending IRQ cannot stop the new transfer.
            WRITE_REG(module->IFCR, DMA_IFCR_CGIF1 << (4U * channel));
            transfer_error[module == DMA2][channel] = false;
            LL_DMA_EnableChannel(module, channel);
        }
        uni_hal_core_irq_resume(primask);
    } else {
        LL_DMA_DisableChannel(module, channel);
    }
    return true;
}


//
// Function/Setters
//


bool uni_hal_dma_set_direction(uni_hal_dma_context_t* ctx, uni_hal_dma_direction_e val)
{
    bool result = false;
    if (uni_hal_dma_is_inited(ctx)) {
        DMA_TypeDef *module = uni_hal_dma_stm32l4_get_module(ctx->config.instance);
        uint32_t channel = uni_hal_dma_stm32l4_get_channel(ctx->config.channel);

        uint32_t direction;
        switch (val) {
        default:
        case UNI_HAL_DMA_DIRECTION_M2M:
            direction = LL_DMA_DIRECTION_MEMORY_TO_MEMORY;
            break;
        case UNI_HAL_DMA_DIRECTION_M2P:
            direction = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
            break;
        case UNI_HAL_DMA_DIRECTION_P2M:
            direction = LL_DMA_DIRECTION_PERIPH_TO_MEMORY;
            break;
        }
        LL_DMA_SetDataTransferDirection(module, channel, direction);

        result = true;
    }

    return result;
}


bool uni_hal_dma_set_fifo_mode(uni_hal_dma_context_t *ctx, bool val) {
    (void)ctx;
    (void)val;
    return false;
}


bool uni_hal_dma_set_mode(uni_hal_dma_context_t *ctx, uni_hal_dma_mode_e val) {
    bool result = false;
    if (uni_hal_dma_is_inited(ctx)) {
        DMA_TypeDef *module = uni_hal_dma_stm32l4_get_module(ctx->config.instance);
        uint32_t channel = uni_hal_dma_stm32l4_get_channel(ctx->config.channel);

        uint32_t mode;
        switch (val) {
        case UNI_HAL_DMA_MODE_CIRCULAR:
            mode = LL_DMA_MODE_CIRCULAR;
            break;
        case UNI_HAL_DMA_MODE_NORMAL:
        default:
            mode = LL_DMA_MODE_NORMAL;
            break;
        }
        LL_DMA_SetMode(module, channel, mode);

        result = true;
    }

    return result;
}


bool uni_hal_dma_set_priority(uni_hal_dma_context_t *ctx, uni_hal_dma_priority_e val) {
    bool result = false;
    if (uni_hal_dma_is_inited(ctx)) {
        DMA_TypeDef *module = uni_hal_dma_stm32l4_get_module(ctx->config.instance);
        uint32_t channel = uni_hal_dma_stm32l4_get_channel(ctx->config.channel);

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

        LL_DMA_SetChannelPriorityLevel(module, channel, priority);
        result = true;
    }

    return result;
}


bool uni_hal_dma_set_memory_align(uni_hal_dma_context_t* ctx, uni_hal_dma_alignment_e val)
{
    bool result = false;
    if (uni_hal_dma_is_inited(ctx))
    {
        DMA_TypeDef *module = uni_hal_dma_stm32l4_get_module(ctx->config.instance);
        uint32_t channel = uni_hal_dma_stm32l4_get_channel(ctx->config.channel);

        uint32_t alignment;
        switch (val)
        {
        default:
        case UNI_HAL_DMA_ALIGNMENT_U8:
            alignment = LL_DMA_MDATAALIGN_BYTE;
            break;
        case UNI_HAL_DMA_ALIGNMENT_U16:
            alignment = LL_DMA_MDATAALIGN_HALFWORD;
            break;
        case UNI_HAL_DMA_ALIGNMENT_U32:
            alignment = LL_DMA_MDATAALIGN_WORD;
            break;
        }

        LL_DMA_SetMemorySize(module, channel, alignment);
        result = true;
    }

    return result;
}

bool uni_hal_dma_set_periph_align(uni_hal_dma_context_t* ctx, uni_hal_dma_alignment_e val)
{
    bool result = false;
    if (uni_hal_dma_is_inited(ctx))
    {
        DMA_TypeDef *module = uni_hal_dma_stm32l4_get_module(ctx->config.instance);
        uint32_t channel = uni_hal_dma_stm32l4_get_channel(ctx->config.channel);

        uint32_t alignment;
        switch (val)
        {
        default:
        case UNI_HAL_DMA_ALIGNMENT_U8:
            alignment = LL_DMA_PDATAALIGN_BYTE;
            break;
        case UNI_HAL_DMA_ALIGNMENT_U16:
            alignment = LL_DMA_PDATAALIGN_HALFWORD;
            break;
        case UNI_HAL_DMA_ALIGNMENT_U32:
            alignment = LL_DMA_PDATAALIGN_WORD;
            break;
        }

        LL_DMA_SetPeriphSize(module, channel, alignment);
        result = true;
    }

    return result;
}

bool uni_hal_dma_set_memory_increment(uni_hal_dma_context_t* ctx, uni_hal_dma_increment_e val)
{
    bool result = false;
    if (uni_hal_dma_is_inited(ctx))
    {
        DMA_TypeDef *module = uni_hal_dma_stm32l4_get_module(ctx->config.instance);
        uint32_t channel = uni_hal_dma_stm32l4_get_channel(ctx->config.channel);

        uint32_t increment;
        switch (val)
        {
        default:
        case UNI_HAL_DMA_INCREMENT_NO:
            increment = LL_DMA_MEMORY_NOINCREMENT;
            break;
        case UNI_HAL_DMA_INCREMENT_YES:
            increment = LL_DMA_MEMORY_INCREMENT;
            break;
        }


        LL_DMA_SetMemoryIncMode(module, channel, increment);
        result = true;
    }

    return result;
}

bool uni_hal_dma_set_periph_increment(uni_hal_dma_context_t* ctx, uni_hal_dma_increment_e val)
{
    bool result = false;
    if (uni_hal_dma_is_inited(ctx))
    {
        DMA_TypeDef *module = uni_hal_dma_stm32l4_get_module(ctx->config.instance);
        uint32_t channel = uni_hal_dma_stm32l4_get_channel(ctx->config.channel);

        uint32_t increment;
        switch (val)
        {
        default:
        case UNI_HAL_DMA_INCREMENT_NO:
            increment = LL_DMA_PERIPH_NOINCREMENT;
            break;
        case UNI_HAL_DMA_INCREMENT_YES:
            increment = LL_DMA_PERIPH_INCREMENT;
            break;
        }

        LL_DMA_SetPeriphIncMode(module, channel, increment);
        result = true;
    }

    return result;
}


bool uni_hal_dma_set_request(uni_hal_dma_context_t* ctx)
{
    bool result = false;
    if (uni_hal_dma_is_inited(ctx))
    {
        DMA_TypeDef *module = uni_hal_dma_stm32l4_get_module(ctx->config.instance);
        uint32_t channel = uni_hal_dma_stm32l4_get_channel(ctx->config.channel);

        LL_DMA_SetPeriphRequest(module, channel, LL_DMA_REQUEST_4);
        result = true;
    }

    return result;
}


bool uni_hal_dma_set_address(uni_hal_dma_context_t* ctx, uint32_t addr_from, uint32_t addr_to)
{
    bool result = false;
    if (uni_hal_dma_is_inited(ctx))
    {
        DMA_TypeDef *module = uni_hal_dma_stm32l4_get_module(ctx->config.instance);
        uint32_t channel = uni_hal_dma_stm32l4_get_channel(ctx->config.channel);
        LL_DMA_ConfigAddresses(module, channel, addr_from, addr_to, LL_DMA_GetDataTransferDirection(module, channel));
        result = true;
    }

    return result;
}

bool uni_hal_dma_set_length(uni_hal_dma_context_t* ctx, uint32_t length)
{
    bool result = false;
    if (uni_hal_dma_is_inited(ctx))
    {
        DMA_TypeDef *module = uni_hal_dma_stm32l4_get_module(ctx->config.instance);
        uint32_t channel = uni_hal_dma_stm32l4_get_channel(ctx->config.channel);

        LL_DMA_SetDataLength(module, channel, length);
        result = true;
    }

    return result;
}
