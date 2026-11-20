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


// RM0351 requires the channel to be disabled before changing its transfer configuration.
// Keep each test and register update together so a preempting task or IRQ cannot start it.
typedef void (*dma_field_setter_t)(const DMA_TypeDef *, uint32_t, uint32_t);

static bool set_channel_field(const uni_hal_dma_context_t *ctx, dma_field_setter_t set, uint32_t value)
{
    if (!uni_hal_dma_is_inited(ctx)) {
        return false;
    }
    DMA_TypeDef *module = uni_hal_dma_stm32l4_get_module(ctx->config.instance);
    uint32_t const channel = uni_hal_dma_stm32l4_get_channel(ctx->config.channel);
    uint32_t const mask = uni_hal_core_irq_pause();
    bool const writable = !LL_DMA_IsEnabledChannel(module, channel);
    if (writable) {
        set(module, channel, value);
    }
    uni_hal_core_irq_resume(mask);
    return writable;
}

bool uni_hal_dma_set_direction(uni_hal_dma_context_t *ctx, uni_hal_dma_direction_e val)
{
    switch (val) {
    default:
    case UNI_HAL_DMA_DIRECTION_M2M:
        return set_channel_field(ctx, LL_DMA_SetDataTransferDirection, LL_DMA_DIRECTION_MEMORY_TO_MEMORY);
    case UNI_HAL_DMA_DIRECTION_M2P:
        return set_channel_field(ctx, LL_DMA_SetDataTransferDirection, LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
    case UNI_HAL_DMA_DIRECTION_P2M:
        return set_channel_field(ctx, LL_DMA_SetDataTransferDirection, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
    }
}


bool uni_hal_dma_set_fifo_mode(uni_hal_dma_context_t *ctx, bool val) {
    (void)ctx;
    (void)val;
    return false;
}


bool uni_hal_dma_set_mode(uni_hal_dma_context_t *ctx, uni_hal_dma_mode_e val)
{
    switch (val) {
    case UNI_HAL_DMA_MODE_CIRCULAR:
        return set_channel_field(ctx, LL_DMA_SetMode, LL_DMA_MODE_CIRCULAR);
    case UNI_HAL_DMA_MODE_NORMAL:
    default:
        return set_channel_field(ctx, LL_DMA_SetMode, LL_DMA_MODE_NORMAL);
    }
}


bool uni_hal_dma_set_priority(uni_hal_dma_context_t *ctx, uni_hal_dma_priority_e val)
{
    switch (val) {
    case UNI_HAL_DMA_PRIORITY_MEDIUM:
        return set_channel_field(ctx, LL_DMA_SetChannelPriorityLevel, LL_DMA_PRIORITY_MEDIUM);
    case UNI_HAL_DMA_PRIORITY_HIGH:
        return set_channel_field(ctx, LL_DMA_SetChannelPriorityLevel, LL_DMA_PRIORITY_HIGH);
    case UNI_HAL_DMA_PRIORITY_VERYHIGH:
        return set_channel_field(ctx, LL_DMA_SetChannelPriorityLevel, LL_DMA_PRIORITY_VERYHIGH);
    case UNI_HAL_DMA_PRIORITY_LOW:
    default:
        return set_channel_field(ctx, LL_DMA_SetChannelPriorityLevel, LL_DMA_PRIORITY_LOW);
    }
}


bool uni_hal_dma_set_memory_align(uni_hal_dma_context_t *ctx, uni_hal_dma_alignment_e val)
{
    switch (val) {
    default:
    case UNI_HAL_DMA_ALIGNMENT_U8:
        return set_channel_field(ctx, LL_DMA_SetMemorySize, LL_DMA_MDATAALIGN_BYTE);
    case UNI_HAL_DMA_ALIGNMENT_U16:
        return set_channel_field(ctx, LL_DMA_SetMemorySize, LL_DMA_MDATAALIGN_HALFWORD);
    case UNI_HAL_DMA_ALIGNMENT_U32:
        return set_channel_field(ctx, LL_DMA_SetMemorySize, LL_DMA_MDATAALIGN_WORD);
    }
}

bool uni_hal_dma_set_periph_align(uni_hal_dma_context_t *ctx, uni_hal_dma_alignment_e val)
{
    switch (val) {
    default:
    case UNI_HAL_DMA_ALIGNMENT_U8:
        return set_channel_field(ctx, LL_DMA_SetPeriphSize, LL_DMA_PDATAALIGN_BYTE);
    case UNI_HAL_DMA_ALIGNMENT_U16:
        return set_channel_field(ctx, LL_DMA_SetPeriphSize, LL_DMA_PDATAALIGN_HALFWORD);
    case UNI_HAL_DMA_ALIGNMENT_U32:
        return set_channel_field(ctx, LL_DMA_SetPeriphSize, LL_DMA_PDATAALIGN_WORD);
    }
}

bool uni_hal_dma_set_memory_increment(uni_hal_dma_context_t *ctx, uni_hal_dma_increment_e val)
{
    switch (val) {
    default:
    case UNI_HAL_DMA_INCREMENT_NO:
        return set_channel_field(ctx, LL_DMA_SetMemoryIncMode, LL_DMA_MEMORY_NOINCREMENT);
    case UNI_HAL_DMA_INCREMENT_YES:
        return set_channel_field(ctx, LL_DMA_SetMemoryIncMode, LL_DMA_MEMORY_INCREMENT);
    }
}

bool uni_hal_dma_set_periph_increment(uni_hal_dma_context_t *ctx, uni_hal_dma_increment_e val)
{
    switch (val) {
    default:
    case UNI_HAL_DMA_INCREMENT_NO:
        return set_channel_field(ctx, LL_DMA_SetPeriphIncMode, LL_DMA_PERIPH_NOINCREMENT);
    case UNI_HAL_DMA_INCREMENT_YES:
        return set_channel_field(ctx, LL_DMA_SetPeriphIncMode, LL_DMA_PERIPH_INCREMENT);
    }
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


bool uni_hal_dma_set_address(uni_hal_dma_context_t *ctx, uint32_t addr_from, uint32_t addr_to)
{
    if (!uni_hal_dma_is_inited(ctx)) {
        return false;
    }
    DMA_TypeDef *module = uni_hal_dma_stm32l4_get_module(ctx->config.instance);
    uint32_t const channel = uni_hal_dma_stm32l4_get_channel(ctx->config.channel);
    uint32_t const mask = uni_hal_core_irq_pause();
    bool const writable = !LL_DMA_IsEnabledChannel(module, channel);
    if (writable) {
        LL_DMA_ConfigAddresses(module, channel, addr_from, addr_to, LL_DMA_GetDataTransferDirection(module, channel));
    }
    uni_hal_core_irq_resume(mask);
    return writable;
}

bool uni_hal_dma_set_length(uni_hal_dma_context_t *ctx, uint32_t length)
{
    return length <= UINT16_MAX && set_channel_field(ctx, LL_DMA_SetDataLength, length);
}
