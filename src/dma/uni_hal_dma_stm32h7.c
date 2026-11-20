//
// Includes
//

// stdlib
#include <stddef.h>

// STM
#include <stm32h7xx_ll_bus.h>
#include <stm32h7xx_ll_dma.h>

// Uni.HAL OS layer
#include "os/uni_hal_os.h"

// uni_hal
#include "dma/uni_hal_dma.h"
#include "dma/uni_hal_dma_stm32h7_irq.h"
#include "core/uni_hal_core.h"
#include "rcc/uni_hal_rcc.h"


//
// Defines
//

#define UNI_HAL_DMA_INTERRUPT_PRIORITY (4U)


//
// ISRs
//

typedef enum {
    DMA_NO_CIRCULAR_CAPTURE,
    DMA_CIRCULAR_RUNNING,
    DMA_CIRCULAR_FAILED
} dma_capture_state_t;

typedef struct {
    const uni_hal_dma_context_t *dma;
    uni_hal_dma_stm32h7_event_fn event;
    void *owner;
    uint32_t saved_interrupts;
    dma_capture_state_t capture;
} dma_listener_t;

static dma_listener_t listeners[2][8];
static const uint8_t flag_shift[4] = {0U, 6U, 16U, 22U};
#define DMA_STREAM_FLAGS (DMA_LISR_TCIF0 | DMA_LISR_HTIF0 | DMA_LISR_TEIF0 | DMA_LISR_DMEIF0 | DMA_LISR_FEIF0)

static void clear_stream_flags(DMA_TypeDef *module, uint32_t stream, uint32_t flags) {
    uint32_t const bits = flags << flag_shift[stream % 4U];
    if (stream < 4U) {
        WRITE_REG(module->LIFCR, bits);
    } else {
        WRITE_REG(module->HIFCR, bits);
    }
}

static DMA_Stream_TypeDef *stream_regs(DMA_TypeDef *module, uint32_t stream) {
    return (DMA_Stream_TypeDef *)((uintptr_t)module + LL_DMA_STR_OFFSET_TAB[stream]);
}

static dma_listener_t *stream_owner(const uni_hal_dma_context_t *dma) {
    if (!dma || (unsigned)dma->config.channel > LL_DMA_STREAM_7) {
        return nullptr;
    }
    DMA_TypeDef *module = uni_hal_dma_stm32h7_get_module(dma->config.instance);
    return module ? &listeners[module == DMA2][dma->config.channel] : nullptr;
}

bool uni_hal_dma_stm32h7_claim(const uni_hal_dma_context_t *dma, uni_hal_dma_stm32h7_event_fn event, void *owner) {
    DMA_TypeDef *module = uni_hal_dma_stm32h7_get_module(dma->config.instance);
    uint32_t const stream = dma->config.channel;
    if (!module || stream > LL_DMA_STREAM_7 || !dma->state.initialized) {
        return false;
    }
    dma_listener_t *slot = &listeners[module == DMA2][stream];
    if (slot->dma || LL_DMA_IsEnabledStream(module, stream)) {
        return false;
    }
    clear_stream_flags(module, stream, DMA_STREAM_FLAGS);
    *slot = (dma_listener_t){.dma = dma, .event = event, .owner = owner,
        .saved_interrupts = stream_regs(module, stream)->CR & (DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE)};
    return true;
}

void uni_hal_dma_stm32h7_circular_started(const uni_hal_dma_context_t *dma) {
    stream_owner(dma)->capture = DMA_CIRCULAR_RUNNING;
}

bool uni_hal_dma_stm32h7_circular_active(const uni_hal_dma_context_t *dma, const void *owner) {
    uint32_t const mask = uni_hal_core_irq_pause();
    const dma_listener_t *slot = stream_owner(dma);
    bool const active = slot && slot->dma == dma && slot->owner == owner && slot->capture == DMA_CIRCULAR_RUNNING &&
        LL_DMA_IsEnabledStream(uni_hal_dma_stm32h7_get_module(dma->config.instance), dma->config.channel);
    uni_hal_core_irq_resume(mask);
    return active;
}

bool uni_hal_dma_stm32h7_release(const uni_hal_dma_context_t *dma) {
    DMA_TypeDef *module = uni_hal_dma_stm32h7_get_module(dma->config.instance);
    uint32_t const stream = dma->config.channel;
    uint32_t const flags = (stream < 4U ? module->LISR : module->HISR) >> flag_shift[stream % 4U];
    clear_stream_flags(module, stream, DMA_STREAM_FLAGS);
    dma_listener_t *slot = &listeners[module == DMA2][stream];
    MODIFY_REG(stream_regs(module, stream)->CR, DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE, slot->saved_interrupts);
    *slot = (dma_listener_t){};
    return (flags & (DMA_LISR_TEIF0 | DMA_LISR_DMEIF0)) != 0U;
}

static void handle_stream_irq(DMA_TypeDef *module, uint32_t stream) {
    uint32_t const mask = uni_hal_core_irq_pause();
    uint32_t const status = stream < 4U ? module->LISR : module->HISR;
    uint32_t const flags = (status >> flag_shift[stream % 4U]) & DMA_STREAM_FLAGS;
    dma_listener_t *listener = &listeners[module == DMA2][stream];
    // Streams used directly by the application can also enable transfer interrupts.
    uint32_t const handled = listener->dma ? flags : flags & (DMA_LISR_TCIF0 | DMA_LISR_TEIF0 | DMA_LISR_DMEIF0);
    if (handled) {
        clear_stream_flags(module, stream, handled);
        uint32_t const fault = handled & (DMA_LISR_TEIF0 | DMA_LISR_DMEIF0);
        if (fault && listener->capture == DMA_CIRCULAR_RUNNING) {
            listener->capture = DMA_CIRCULAR_FAILED;
            LL_DMA_DisableStream(module, stream);
        }
        if (listener->event && (fault || (handled & DMA_LISR_TCIF0))) {
            listener->event(listener->owner, listener->dma, fault != 0U);
        }
    }
    uni_hal_core_irq_resume(mask);
}

#define UNI_HAL_DMA_IRQ_HANDLER(module, stream)              \
    void module##_Stream##stream##_IRQHandler(void) {        \
        UNI_HAL_OS_ISR_ENTER();                              \
        handle_stream_irq(module, stream);                   \
        UNI_HAL_OS_ISR_EXIT(pdFALSE);                        \
    }

UNI_HAL_DMA_IRQ_HANDLER(DMA1, 0)
UNI_HAL_DMA_IRQ_HANDLER(DMA1, 1)
UNI_HAL_DMA_IRQ_HANDLER(DMA1, 2)
UNI_HAL_DMA_IRQ_HANDLER(DMA1, 3)
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
    uint32_t const mask = uni_hal_core_irq_pause();
    dma_listener_t *slot = stream_owner(ctx);
    if (!slot || (slot->dma && slot->dma != ctx)) {
        uni_hal_core_irq_resume(mask);
        return false;
    }
    if (!ctx->state.initialized && uni_hal_rcc_clk_set(ctx->config.instance, true)) {
        uint32_t const interrupt = _uni_hal_dma_get_interrupt(ctx->config.instance, ctx->config.channel);
        NVIC_SetPriority(interrupt, UNI_HAL_DMA_INTERRUPT_PRIORITY);
        NVIC_EnableIRQ(interrupt);
        ctx->state.initialized = true;
    }
    uni_hal_core_irq_resume(mask);
    return ctx->state.initialized;
}


bool uni_hal_dma_enable(uni_hal_dma_context_t *ctx, bool val) {
    uint32_t const mask = uni_hal_core_irq_pause();
    dma_listener_t *slot = stream_owner(ctx);
    bool ok = false;
    // The ADC owner can explicitly stop its circular capture using the original descriptor.
    if (slot && ctx->state.initialized && (!slot->dma || (!val && slot->dma == ctx && slot->capture != DMA_NO_CIRCULAR_CAPTURE))) {
        DMA_TypeDef *module = uni_hal_dma_stm32h7_get_module(ctx->config.instance);
        uint32_t const stream = ctx->config.channel;
        if (val) {
            LL_DMA_EnableStream(module, stream);
            ok = true;
        } else {
            LL_DMA_DisableStream(module, stream);
            ok = !LL_DMA_IsEnabledStream(module, stream);
            if (ok && slot->dma) {
                uni_hal_dma_stm32h7_release(ctx);
            }
        }
    }
    uni_hal_core_irq_resume(mask);
    return ok;
}


//
// Functions/Setters
//

bool uni_hal_dma_set_fifo_mode(uni_hal_dma_context_t *ctx, bool val) {
    bool result = false;
    uint32_t const mask = uni_hal_core_irq_pause();
    dma_listener_t *slot = stream_owner(ctx);
    if (slot && ctx->state.initialized && !slot->dma) {
        DMA_TypeDef *module = uni_hal_dma_stm32h7_get_module(ctx->config.instance);
        uint32_t stream = uni_hal_dma_stm32h7_get_channel(ctx->config.channel);
        val != false ? LL_DMA_EnableFifoMode(module, stream) : LL_DMA_DisableFifoMode(module, stream);
        result = true;
    }
    uni_hal_core_irq_resume(mask);
    return result;
}


bool uni_hal_dma_set_mode(uni_hal_dma_context_t *ctx, uni_hal_dma_mode_e val) {
    bool result = false;
    uint32_t const mask = uni_hal_core_irq_pause();
    dma_listener_t *slot = stream_owner(ctx);
    if (slot && ctx->state.initialized && !slot->dma) {
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

    uni_hal_core_irq_resume(mask);
    return result;
}


bool uni_hal_dma_set_priority(uni_hal_dma_context_t *ctx, uni_hal_dma_priority_e val) {
    bool result = false;
    uint32_t const mask = uni_hal_core_irq_pause();
    dma_listener_t *slot = stream_owner(ctx);
    if (slot && ctx->state.initialized && !slot->dma) {
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

    uni_hal_core_irq_resume(mask);
    return result;
}
