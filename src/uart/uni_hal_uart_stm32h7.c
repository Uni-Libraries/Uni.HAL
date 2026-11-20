//
// Includes
//

// stdlib
#include <stdint.h>
#include <string.h>

// ST
#include <stm32h7xx_ll_usart.h>

// uni_hal
#include "core/uni_hal_core.h"
#include "rcc/uni_hal_rcc_stm32h7_internal.h"
#include "io/uni_hal_io_tunnel.h"
#include "systick/uni_hal_systick.h"
#include "os/uni_hal_os.h"
#include "os/uni_hal_os_irq_stm32.h"
#include "uart/uni_hal_uart.h"



//
// Defines
//

#define UNI_HAL_UART_NUM (8U)

//
// Globals
//

static uni_hal_usart_context_t * g_uni_hal_usart_ctx[UNI_HAL_UART_NUM] = { 0 };
static bool configuring[UNI_HAL_UART_NUM];



//
// Private
//

static USART_TypeDef *_uni_hal_uart_handle_get(uni_hal_core_periph_e instance) {
    USART_TypeDef *result = nullptr;
    switch (instance) {
    case UNI_HAL_CORE_PERIPH_UART_1:
        result = USART1;
        break;
    case UNI_HAL_CORE_PERIPH_UART_2:
        result = USART2;
        break;
    case UNI_HAL_CORE_PERIPH_UART_3:
        result = USART3;
        break;
    case UNI_HAL_CORE_PERIPH_UART_4:
        result = UART4;
        break;
    case UNI_HAL_CORE_PERIPH_UART_5:
        result = UART5;
        break;
    case UNI_HAL_CORE_PERIPH_UART_6:
        result = USART6;
        break;
    case UNI_HAL_CORE_PERIPH_UART_7:
        result = UART7;
        break;
    case UNI_HAL_CORE_PERIPH_UART_8:
        result = UART8;
        break;
    default:
        break;
    }
    return result;
}

// STM32H7 USART has FIFO support and Cube LL can expose RXNE/RXFNE with different names.
#if defined(LL_USART_IsActiveFlag_RXNE_RXFNE)
#define UNI_HAL_USART_IsActiveFlag_RXNE(handle) LL_USART_IsActiveFlag_RXNE_RXFNE(handle)
#define UNI_HAL_USART_IsEnabledIT_RXNE(handle)  LL_USART_IsEnabledIT_RXNE_RXFNE(handle)
#define UNI_HAL_USART_EnableIT_RXNE(handle)     LL_USART_EnableIT_RXNE_RXFNE(handle)
#define UNI_HAL_USART_DisableIT_RXNE(handle)    LL_USART_DisableIT_RXNE_RXFNE(handle)
#else
#define UNI_HAL_USART_IsActiveFlag_RXNE(handle) LL_USART_IsActiveFlag_RXNE(handle)
#define UNI_HAL_USART_IsEnabledIT_RXNE(handle)  LL_USART_IsEnabledIT_RXNE(handle)
#define UNI_HAL_USART_EnableIT_RXNE(handle)     LL_USART_EnableIT_RXNE(handle)
#define UNI_HAL_USART_DisableIT_RXNE(handle)    LL_USART_DisableIT_RXNE(handle)
#endif

/**
 * Longest wait for the transmitter and receiver to acknowledge that they are enabled
 */
#define UNI_HAL_USART_ENABLE_TIMEOUT_MS (100U)

/**
 * Wait until the directions that are switched on report being enabled
 * @param handle USART or LPUART instance
 * @return false when the peripheral did not come up in time, e.g. without a kernel clock
 */
static bool _uni_hal_usart_wait_enabled(const USART_TypeDef *handle) {
    uint32_t const direction = LL_USART_GetTransferDirection(handle);
    bool const wait_tx = (direction & LL_USART_DIRECTION_TX) != 0U;
    bool const wait_rx = (direction & LL_USART_DIRECTION_RX) != 0U;

    uint32_t const start_ms = uni_hal_systick_get_ms();
    while ((wait_tx && LL_USART_IsActiveFlag_TEACK(handle) == 0U) ||
           (wait_rx && LL_USART_IsActiveFlag_REACK(handle) == 0U)) {
        if ((uni_hal_systick_get_ms() - start_ms) > UNI_HAL_USART_ENABLE_TIMEOUT_MS) {
            return false;
        }
    }

    return true;
}


/**
 * Enable NVIC interrupts
 * @param instance UART instance
 * @param priority interrupt priority
 */
static bool _uni_hal_uart_nvic(uni_hal_core_periph_e instance, uint32_t priority) {
    bool result = false;

    uni_hal_core_irq_e irq;
    switch (instance) {
        case UNI_HAL_CORE_PERIPH_UART_1:
            irq = UNI_HAL_CORE_IRQ_UART_1;
            break;
        case UNI_HAL_CORE_PERIPH_UART_2:
            irq = UNI_HAL_CORE_IRQ_UART_2;
            break;
        case UNI_HAL_CORE_PERIPH_UART_3:
            irq = UNI_HAL_CORE_IRQ_UART_3;
            break;
        case UNI_HAL_CORE_PERIPH_UART_4:
            irq = UNI_HAL_CORE_IRQ_UART_4;
            break;
        case UNI_HAL_CORE_PERIPH_UART_5:
            irq = UNI_HAL_CORE_IRQ_UART_5;
            break;
        case UNI_HAL_CORE_PERIPH_UART_6:
            irq = UNI_HAL_CORE_IRQ_UART_6;
            break;
        case UNI_HAL_CORE_PERIPH_UART_7:
            irq = UNI_HAL_CORE_IRQ_UART_7;
            break;
        case UNI_HAL_CORE_PERIPH_UART_8:
            irq = UNI_HAL_CORE_IRQ_UART_8;
            break;
        default:
            irq = UNI_HAL_CORE_IRQ_UNKNOWN;
            break;
    }

    if (irq != UNI_HAL_CORE_IRQ_UNKNOWN) {
        uni_hal_core_irq_enable(irq, priority, 0);
        result = true;
    }

    return result;
}


// Reserve the physical port and its clock before GPIO setup and baud-rate calculation.
static bool claim_clock(const uni_hal_usart_context_t *ctx, unsigned index) {
    uint32_t const mask = uni_hal_core_irq_pause();
    bool available = !configuring[index];
    if (available && ctx->clksrc != UNI_HAL_RCC_CLKSRC_UNKNOWN &&
        ctx->clksrc != uni_hal_rcc_clksrc_get(ctx->instance)) {
        bool const apb2 = uart_uses_apb2(ctx->instance);
        for (unsigned i = 0U; i < UNI_HAL_UART_NUM; ++i) {
            uni_hal_core_periph_e const peer = (uni_hal_core_periph_e)(UNI_HAL_CORE_PERIPH_UART_1 + i);
            if (i != index && uart_uses_apb2(peer) == apb2 && (configuring[i] || g_uni_hal_usart_ctx[i])) {
                available = false;
                break;
            }
        }
        if (available) {
            available = uni_hal_rcc_clksrc_set(ctx->instance, ctx->clksrc);
        }
    }
    if (available) {
        configuring[index] = true;
    }
    uni_hal_core_irq_resume(mask);
    return available;
}


//
// IO handlers
//

static void _uni_hal_usart_tx_trigger(void *UNI_COMMON_COMPILER_UNUSED_VAR(ctx_io_r), void *ctx_fn_r) {
    if (ctx_fn_r != nullptr) {
        uni_hal_usart_context_t *ctx_fn = (uni_hal_usart_context_t *)ctx_fn_r;
        USART_TypeDef *instance = _uni_hal_uart_handle_get(ctx_fn->instance);
        if (instance != nullptr) {
            // A previous IRQ can already have consumed the byte just enqueued. Preserve its TC.
            const uint32_t mask = uni_hal_core_irq_pause();
            if (UNI_HAL_USART_IsEnabledIT_RXNE(instance)) { // Driver IRQ sources are enabled as a group.
                LL_USART_EnableIT_TXE_TXFNF(instance);
            }
            uni_hal_core_irq_resume(mask);
        }
    }
}



//
// IRQ handlers
//

static BaseType_t _uni_hal_usart_irq_handler(uni_hal_usart_context_t *ctx) {
    BaseType_t higher_task_woken = pdFALSE;

    if (ctx == nullptr) {
        return pdFALSE;
    }

    USART_TypeDef *dev_handle = _uni_hal_uart_handle_get(ctx->instance);
    if (dev_handle == nullptr) {
        return pdFALSE;
    }

    uni_hal_io_context_t *ctx_io = ctx->io;
    if (ctx_io == nullptr) {
        return pdFALSE;
    }

    if (LL_USART_IsActiveFlag_ORE(dev_handle)) {
        ctx_io->stats.rx_overrun++;
        LL_USART_ClearFlag_ORE(dev_handle);
    }

    if (UNI_HAL_USART_IsEnabledIT_RXNE(dev_handle) && UNI_HAL_USART_IsActiveFlag_RXNE(dev_handle)) {
        // receive data
        uint8_t data = LL_USART_ReceiveData8(dev_handle);

        // try to tunnel data
        uni_hal_io_tunnel_transmit_result_t tunnel_result = UNI_HAL_IO_TUNNEL_TRANSPARENT;
        if (ctx_io->handlers.tunnel_ctx) {
            tunnel_result = uni_hal_io_tunnel_transmit(ctx_io->handlers.tunnel_ctx, ctx_io, &data, sizeof(data));
        }

        // send to our ringbuffer in case we failed to tunnel data
        if (tunnel_result == UNI_HAL_IO_TUNNEL_TRANSPARENT || tunnel_result == UNI_HAL_IO_TUNNEL_FAIL) {
            if (uni_hal_io_buffer_push_isr(&ctx_io->buf_rx, &data, 1U, &higher_task_woken)) {
                ctx_io->stats.rx_received++;
            }
            else {
                ctx_io->stats.rx_overrun++;
            }
        }
    }

    if (LL_USART_IsEnabledIT_TXE_TXFNF(dev_handle) && LL_USART_IsActiveFlag_TXE_TXFNF(dev_handle)) {
        uint8_t data = 0U;

        // Move the byte from software to the active transmission without exposing an idle gap.
        const uint32_t mask = uni_hal_core_irq_pause();
        const bool have_byte = uni_hal_io_buffer_pop_isr(&ctx_io->buf_tx, &data, 1U, &higher_task_woken) != 0U;
        const bool starting = have_byte && !ctx->in_transmission;
        if (starting) {
            ctx->in_transmission = true;
        }
        uni_hal_core_irq_resume(mask);

        if (have_byte) {
            // We have data to send -> TXE path is active, TC interrupt must be off.
            LL_USART_DisableIT_TC(dev_handle);

            if (starting && ctx_io->handlers.tx_start) {
                ctx_io->handlers.tx_start(ctx_io, ctx_io->handlers.tx_start_ctx);
            }

            // Only a committed next byte supersedes a completion already latched in the hardware.
            LL_USART_ClearFlag_TC(dev_handle);
            LL_USART_TransmitData8(dev_handle, data);
            ctx_io->stats.tx_transmited++;
        }
        else {
            // No more bytes to write into TDR -> wait for last byte to fully shift out.
            LL_USART_DisableIT_TXE_TXFNF(dev_handle);
            if (ctx->in_transmission) {
                LL_USART_EnableIT_TC(dev_handle);
            }
            // A higher-priority producer may have triggered TXE between the pop and the mask.
            if (!uni_hal_io_buffer_is_empty(&ctx_io->buf_tx)) {
                LL_USART_EnableIT_TXE_TXFNF(dev_handle);
            }
        }
    }

    // Transfer Complete
    if (LL_USART_IsEnabledIT_TC(dev_handle) && LL_USART_IsActiveFlag_TC(dev_handle)) {
        // NOTE:
        // - TXE_TXFNF is a *status* flag (mapped to TXE on STM32H7). It is not "clearable".
        //   Correct handling is enabling/disabling its interrupt source (TXEIE_TXFNFIE).
        // - TC is clearable via ICR (LL_USART_ClearFlag_TC).
        LL_USART_ClearFlag_TC(dev_handle);

        if (!ctx->in_transmission) {
            // Nothing to complete, but do not keep TC interrupt enabled (TC is often 1 when idle).
            LL_USART_DisableIT_TC(dev_handle);
        }
        else if (uni_hal_io_buffer_is_empty(&ctx_io->buf_tx)) {
            // A concurrent enqueue can already have re-enabled TXE for the next burst.
            LL_USART_DisableIT_TC(dev_handle);
            ctx->in_transmission = false;

            if (ctx_io->handlers.tx_end) {
                ctx_io->handlers.tx_end(ctx_io, ctx_io->handlers.tx_end_ctx);
            }

            // callback on transfer complete
            if (ctx->callback) {
                if (ctx->callback(ctx, ctx->callback_cookie, UNI_HAL_USART_CALLBACK_TC)) {
                    higher_task_woken = pdTRUE;
                }
            }
        }
        else {
            // New data appeared while we were waiting for TC -> continue sending.
            LL_USART_DisableIT_TC(dev_handle);
            LL_USART_EnableIT_TXE_TXFNF(dev_handle);
        }
    }

    // Idle
    if (LL_USART_IsEnabledIT_IDLE(dev_handle) && LL_USART_IsActiveFlag_IDLE(dev_handle)) {
        LL_USART_ClearFlag_IDLE(dev_handle);
        if (ctx->callback) {
            if (ctx->callback(ctx, ctx->callback_cookie, UNI_HAL_USART_CALLBACK_IDLE)) {
                higher_task_woken = pdTRUE;
            }
        }
    }

    return higher_task_woken;
}

void USART1_IRQHandler(void) {
     UNI_HAL_OS_ISR_ENTER();
    UNI_HAL_OS_ISR_EXIT(_uni_hal_usart_irq_handler(g_uni_hal_usart_ctx[0]));
}

void USART2_IRQHandler(void) {
    UNI_HAL_OS_ISR_ENTER();
    UNI_HAL_OS_ISR_EXIT(_uni_hal_usart_irq_handler(g_uni_hal_usart_ctx[1]));
}

void USART3_IRQHandler(void) {
    UNI_HAL_OS_ISR_ENTER();
    UNI_HAL_OS_ISR_EXIT(_uni_hal_usart_irq_handler(g_uni_hal_usart_ctx[2]));
}

void UART4_IRQHandler(void) {
    UNI_HAL_OS_ISR_ENTER();
    UNI_HAL_OS_ISR_EXIT(_uni_hal_usart_irq_handler(g_uni_hal_usart_ctx[3]));
}

void UART5_IRQHandler(void) {
    UNI_HAL_OS_ISR_ENTER();
    UNI_HAL_OS_ISR_EXIT(_uni_hal_usart_irq_handler(g_uni_hal_usart_ctx[4]));
}

void USART6_IRQHandler(void) {
    UNI_HAL_OS_ISR_ENTER();
    UNI_HAL_OS_ISR_EXIT(_uni_hal_usart_irq_handler(g_uni_hal_usart_ctx[5]));
}

void UART7_IRQHandler(void) {
    UNI_HAL_OS_ISR_ENTER();
    UNI_HAL_OS_ISR_EXIT(_uni_hal_usart_irq_handler(g_uni_hal_usart_ctx[6]));
}

void UART8_IRQHandler(void) {
    UNI_HAL_OS_ISR_ENTER();
    UNI_HAL_OS_ISR_EXIT(_uni_hal_usart_irq_handler(g_uni_hal_usart_ctx[7]));
}



//
// Public
//

bool uni_hal_usart_init(uni_hal_usart_context_t *ctx) {
    bool result = false;
    // the interrupt handler needs the IO context to store what it receives
    if (ctx != nullptr && ctx->pin_rx != nullptr && ctx->pin_tx != nullptr && ctx->io != nullptr &&
        uni_hal_os_irq_priority_valid(ctx->isr_priority)) {
        unsigned const index = (unsigned)ctx->instance - UNI_HAL_CORE_PERIPH_UART_1;
        if (index >= UNI_HAL_UART_NUM || !claim_clock(ctx, index)) {
            return false;
        }
        ctx->callback = nullptr;
        ctx->callback_cookie = nullptr;
        ctx->in_transmission = false;
        ctx->initialized = false;

        result = uni_hal_rcc_clk_set(ctx->instance, true);

        // pins
        result = uni_hal_gpio_pin_init(ctx->pin_rx) && result;
        result = uni_hal_gpio_pin_init(ctx->pin_tx) && result;

        // enable IRQ
        result = _uni_hal_uart_nvic(ctx->instance, ctx->isr_priority) && result;

        USART_TypeDef* handle = _uni_hal_uart_handle_get(ctx->instance);
        if(result && handle != nullptr) {
            // init
            LL_USART_InitTypeDef USART_InitStruct = {0};
            USART_InitStruct.PrescalerValue = LL_USART_PRESCALER_DIV1; //-V1048
            USART_InitStruct.BaudRate = ctx->baudrate;
            USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
            USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
            USART_InitStruct.Parity = LL_USART_PARITY_NONE;
            USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
            USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
            USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
            LL_USART_DeInit(handle);
            g_uni_hal_usart_ctx[index] = nullptr;
            result = LL_USART_Init(handle, &USART_InitStruct) == SUCCESS;
            if (result) {
                result = uni_hal_io_init(ctx->io);
            }
            if (result) {
                // enable
                LL_USART_ConfigAsyncMode(handle);

                if (ctx->baudrate_div) {
                    handle->BRR = (ctx->baudrate_div & 0xFFFFU);
                }

                LL_USART_DisableIT_TXE_TXFNF(handle);
                LL_USART_DisableIT_TC(handle);
                LL_USART_ClearFlag_TC(handle);
                LL_USART_ClearFlag_IDLE(handle);

                LL_USART_Enable(handle);

                result = _uni_hal_usart_wait_enabled(handle);
                if (result) {
                    ctx->io->handlers.tx_trigger = _uni_hal_usart_tx_trigger;
                    ctx->io->handlers.tx_trigger_ctx = ctx;
                    g_uni_hal_usart_ctx[index] = ctx;
                    LL_USART_EnableIT_IDLE(handle);
                    UNI_HAL_USART_EnableIT_RXNE(handle);
                } else {
                    LL_USART_Disable(handle);
                    uni_hal_io_deinit(ctx->io);
                }
            }
        }

        ctx->initialized = result;
        uint32_t const mask = uni_hal_core_irq_pause();
        configuring[index] = false;
        uni_hal_core_irq_resume(mask);
    }

    return result;
}


uint32_t uni_hal_usart_baudrate_get(uni_hal_usart_context_t *ctx) {
    uint32_t result = 0;

    if (uni_hal_uart_is_inited(ctx)) {
        USART_TypeDef *instance = _uni_hal_uart_handle_get(ctx->instance);
        if (instance != nullptr) {
            result = LL_USART_GetBaudRate(instance, uni_hal_rcc_clk_get_freq(ctx->instance), LL_USART_GetPrescaler(instance),
                                          LL_USART_GetOverSampling(instance));
        }
    }

    return result;
}

bool uni_hal_usart_baudrate_set(uni_hal_usart_context_t *ctx, uint32_t baudrate) {
    bool result = false;

    if (uni_hal_uart_is_inited(ctx)) {
        USART_TypeDef* instance = _uni_hal_uart_handle_get(ctx->instance);
        if (instance != nullptr) {
            LL_USART_Disable(instance);

            LL_USART_SetBaudRate(instance, uni_hal_rcc_clk_get_freq(ctx->instance),
                                LL_USART_GetPrescaler(instance),
                                LL_USART_GetOverSampling(instance), baudrate);

            LL_USART_Enable(instance);
            result = _uni_hal_usart_wait_enabled(instance);
        }
    }

    return result;
}

bool uni_hal_usart_receive_enable(uni_hal_usart_context_t *ctx, bool value) {
    bool result = false;

    if (uni_hal_uart_is_inited(ctx)) {
        USART_TypeDef *instance = _uni_hal_uart_handle_get(ctx->instance);
        if (instance != nullptr) {
            if (value) {
                LL_USART_EnableDirectionRx(instance);
            } else {
                LL_USART_DisableDirectionRx(instance);
            }
            result = true;
        }
    }

    return result;
}

bool uni_hal_usart_transmit_enable(uni_hal_usart_context_t *ctx, bool value) {
    bool result = false;

    if (uni_hal_uart_is_inited(ctx)) {
        USART_TypeDef *instance = _uni_hal_uart_handle_get(ctx->instance);
        if (instance != nullptr) {  
            if (value) {
                LL_USART_EnableDirectionTx(instance);
            } else {
                LL_USART_DisableDirectionTx(instance);
            }
            result = true;
        }
    }

    return result;
}

bool uni_hal_usart_irq_enable(uni_hal_usart_context_t *ctx, bool value) {
    bool result = false;

    if (uni_hal_uart_is_inited(ctx)) {
        USART_TypeDef *instance = _uni_hal_uart_handle_get(ctx->instance);
        if (instance != nullptr) {
            const uint32_t mask = uni_hal_core_irq_pause();
            if (value) {
                LL_USART_EnableIT_IDLE(instance);
                UNI_HAL_USART_EnableIT_RXNE(instance);
                if (ctx->in_transmission || !uni_hal_io_buffer_is_empty(&ctx->io->buf_tx)) {
                    LL_USART_EnableIT_TXE_TXFNF(instance);
                }
            } else {
                UNI_HAL_USART_DisableIT_RXNE(instance);
                LL_USART_DisableIT_IDLE(instance);
                LL_USART_DisableIT_TXE_TXFNF(instance);
                LL_USART_DisableIT_TC(instance);
                // An in-flight byte can finish while masked. Leave TC and the RS-485 state intact.
                LL_USART_ClearFlag_IDLE(instance);
                LL_USART_ClearFlag_ORE(instance);
            }
            uni_hal_core_irq_resume(mask);

            result = true;
        }
    }

    return result;
}
