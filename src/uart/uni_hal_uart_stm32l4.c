//
// Includes
//

// stdlib
#include <stdint.h>
#include <string.h>

// ST
#include <stm32l4xx.h>
#include <stm32l4xx_ll_lpuart.h>
#include <stm32l4xx_ll_usart.h>

// Uni.Common
#include <uni_common.h>

// Uni.HAL
#include "gpio/uni_hal_gpio.h"
#include "core/uni_hal_core.h"
#include "io/uni_hal_io_tunnel.h"
#include "rcc/uni_hal_rcc.h"
#include "uart/uni_hal_uart.h"


//
// Globals
//

static uni_hal_usart_context_t *_uni_hal_lpuart_1_ctx = NULL;
static uni_hal_usart_context_t *_uni_hal_usart_1_ctx = NULL;
static uni_hal_usart_context_t *_uni_hal_usart_2_ctx = NULL;
static uni_hal_usart_context_t *_uni_hal_usart_3_ctx = NULL;
static uni_hal_usart_context_t *_uni_hal_uart_4_ctx = NULL;
static uni_hal_usart_context_t *_uni_hal_uart_5_ctx = NULL;


//
// Private
//

static void* _uni_hal_uart_handle_get(uni_hal_core_periph_e instance) {
    void* result = NULL;
    switch (instance) {
    case UNI_HAL_CORE_PERIPH_LPUART_1:
        result = LPUART1;
        break;
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
    default:
        break;
    }
    return result;
}

/**
 * Enable NVIC interrupts
 * @param instance UART instance
 * @param priority interrupt priority
 */
static bool _uni_hal_uart_nvic(uni_hal_core_periph_e instance, uint32_t priority) {
    bool result = false;

    switch (instance) {
    case UNI_HAL_CORE_PERIPH_LPUART_1:
        NVIC_SetPriority(LPUART1_IRQn, priority);
        NVIC_EnableIRQ(LPUART1_IRQn);
        result = true;
        break;
    case UNI_HAL_CORE_PERIPH_UART_1:
        NVIC_SetPriority(USART1_IRQn, priority);
        NVIC_EnableIRQ(USART1_IRQn);
        result = true;
        break;
    case UNI_HAL_CORE_PERIPH_UART_2:
        NVIC_SetPriority(USART2_IRQn, priority);
        NVIC_EnableIRQ(USART2_IRQn);
        result = true;
        break;
    case UNI_HAL_CORE_PERIPH_UART_3:
        NVIC_SetPriority(USART3_IRQn, priority);
        NVIC_EnableIRQ(USART3_IRQn);
        result = true;
        break;
    case UNI_HAL_CORE_PERIPH_UART_4:
        NVIC_SetPriority(UART4_IRQn, priority);
        NVIC_EnableIRQ(UART4_IRQn);
        result = true;
        break;
    case UNI_HAL_CORE_PERIPH_UART_5:
        NVIC_SetPriority(UART5_IRQn, priority);
        NVIC_EnableIRQ(UART5_IRQn);
        result = true;
        break;
    default:
        break;
    }
    return result;
}


/**
 * Store given USART context in context storage
 * @param ctx pointer to the USART interface context
 * @return true on success
 */
static bool _uni_hal_usart_save_ctx(uni_hal_usart_context_t *ctx) {
    bool result = false;

    if (ctx != NULL) {
        switch (ctx->instance){
        case UNI_HAL_CORE_PERIPH_LPUART_1:
            _uni_hal_lpuart_1_ctx = ctx;
            result = true;
            break;
        case UNI_HAL_CORE_PERIPH_UART_1:
            _uni_hal_usart_1_ctx = ctx;
            result = true;
            break;
        case UNI_HAL_CORE_PERIPH_UART_2:
            _uni_hal_usart_2_ctx = ctx;
            result = true;
            break;
        case UNI_HAL_CORE_PERIPH_UART_3:
            _uni_hal_usart_3_ctx = ctx;
            result = true;
            break;
        case UNI_HAL_CORE_PERIPH_UART_4:
            _uni_hal_uart_4_ctx = ctx;
            result = true;
            break;
        case UNI_HAL_CORE_PERIPH_UART_5:
            _uni_hal_uart_5_ctx = ctx;
            result = true;
            break;
        default:
            break;
        }
    }

    return result;
}


/**
 * Enables or disables USART Receive Interrupt
 * @param ctx pointer to the interface context
 * @param val true on enable interrupt, false to disable it
 * @return true on success
 */
static bool _uni_hal_usart_irq_rx_enable(uni_hal_usart_context_t *ctx, bool val) {
    bool result = false;
    void* handle = _uni_hal_uart_handle_get(ctx->instance);

    if (handle != nullptr)
    {
        if (handle == LPUART1)
        {
            val ? LL_LPUART_EnableIT_RXNE(handle) : LL_LPUART_DisableIT_RXNE(handle);
        }
        else
        {
            val ? LL_USART_EnableIT_RXNE(handle) : LL_USART_DisableIT_RXNE(handle);
        }
        result = true;
    }

    return result;
}

static bool _uni_hal_usart_irq_tx_enable(uni_hal_usart_context_t *ctx, bool val) {
    bool result = false;

    void* handle = _uni_hal_uart_handle_get(ctx->instance);
    if (handle != NULL) {
        if (handle == LPUART1)
        {
            if (val) {
                LL_LPUART_ClearFlag_TC(handle);
                LL_LPUART_EnableIT_TXE(handle);
                LL_LPUART_EnableIT_TC(handle);
            } else {
                LL_LPUART_DisableIT_TXE(handle);
                LL_LPUART_DisableIT_TC(handle);
                LL_LPUART_ClearFlag_TC(handle);
            }
        }
        else
        {
            if (val) {
                LL_USART_ClearFlag_TC(handle);
                LL_USART_EnableIT_TXE(handle);
                LL_USART_EnableIT_TC(handle);
            } else {
                LL_USART_DisableIT_TXE(handle);
                LL_USART_DisableIT_TC(handle);
                LL_USART_ClearFlag_TC(handle);
            }
        }
        result = true;
    }

    return result;
}


//
// IO handlers
//

static void _uni_hal_usart_tx_trigger(void *UNI_COMMON_COMPILER_UNUSED_VAR(ctx_io_r), void *ctx_fn_r) {
    uni_hal_usart_context_t *ctx_fn = (uni_hal_usart_context_t *)ctx_fn_r;
    _uni_hal_usart_irq_tx_enable(ctx_fn, true);
}


//
// IRQ handlers
//

static bool _uni_hal_usart_irq_handler_lpuart(uni_hal_usart_context_t *ctx) {
    BaseType_t higher_task_woken = 0U;

    void* dev_handle = _uni_hal_uart_handle_get(ctx->instance);
    uni_hal_io_context_t *ctx_io = ctx->io;

    if (LL_LPUART_IsActiveFlag_ORE(dev_handle)) {
        ctx_io->stats.rx_overrun++;
        LL_LPUART_ClearFlag_ORE(dev_handle);
    }

    if (LL_LPUART_IsEnabledIT_RXNE(dev_handle) && LL_LPUART_IsActiveFlag_RXNE(dev_handle)) {
        // receive data
        uint8_t data = LL_LPUART_ReceiveData8(dev_handle);

        // try to tunnel data
        uni_hal_io_tunnel_transmit_result_t tunnel_result =
            uni_hal_io_tunnel_transmit(ctx_io->handlers.tunnel_ctx, ctx_io, &data, sizeof(data));

        // send to our ringbuffer in case we failed to tunnel data
        if (tunnel_result == UNI_HAL_IO_TUNNEL_FAIL || tunnel_result == UNI_HAL_IO_TUNNEL_TRANSPARENT) {
            if (xStreamBufferSendFromISR(ctx_io->buf_rx.handle, &data, 1U, &higher_task_woken)) {
                ctx_io->stats.rx_received++;
            }
            else {
                ctx_io->stats.rx_overrun++;
            }
        }
    }

    if (LL_LPUART_IsEnabledIT_TXE(dev_handle) && LL_LPUART_IsActiveFlag_TXE(dev_handle)) {
        uint8_t data = 0;

        if (!ctx->in_transmission) {
            if (ctx_io->handlers.tx_start) {
                ctx_io->handlers.tx_start(ctx_io, ctx_io->handlers.tx_start_ctx);
            }
            ctx->in_transmission = true;
        }

        if (xStreamBufferReceiveFromISR(ctx_io->buf_tx.handle, &data, 1U, &higher_task_woken)) {
            LL_LPUART_TransmitData8(dev_handle, data);
            ctx_io->stats.tx_transmited++;
        }
    }

    if (LL_LPUART_IsEnabledIT_TC(dev_handle) && LL_LPUART_IsActiveFlag_TC(dev_handle)) {
        if (ctx->in_transmission) {
            _uni_hal_usart_irq_tx_enable(ctx, false);
            ctx->in_transmission = false;

            if (ctx_io->handlers.tx_end) {
                ctx_io->handlers.tx_end(ctx_io, ctx_io->handlers.tx_end_ctx);
            }
        }
    }

    return higher_task_woken;
}

static bool _uni_hal_usart_irq_handler(uni_hal_usart_context_t *ctx) {
    BaseType_t higher_task_woken = 0U;

    USART_TypeDef *dev_handle = _uni_hal_uart_handle_get(ctx->instance);
    uni_hal_io_context_t *ctx_io = ctx->io;

    if (LL_USART_IsActiveFlag_ORE(dev_handle)) {
        ctx_io->stats.rx_overrun++;
        LL_USART_ClearFlag_ORE(dev_handle);
    }

    if (LL_USART_IsEnabledIT_RXNE(dev_handle) && LL_USART_IsActiveFlag_RXNE(dev_handle)) {
        // receive data
        uint8_t data = LL_USART_ReceiveData8(dev_handle);

        // try to tunnel data
        uni_hal_io_tunnel_transmit_result_t tunnel_result =
            uni_hal_io_tunnel_transmit(ctx_io->handlers.tunnel_ctx, ctx_io, &data, sizeof(data));

        // send to our ringbuffer in case we failed to tunnel data
        if (tunnel_result == UNI_HAL_IO_TUNNEL_FAIL || tunnel_result == UNI_HAL_IO_TUNNEL_TRANSPARENT) {
            if (xStreamBufferSendFromISR(ctx_io->buf_rx.handle, &data, 1U, &higher_task_woken)) {
                ctx_io->stats.rx_received++;
            }
            else {
                ctx_io->stats.rx_overrun++;
            }
        }
    }

    if (LL_USART_IsEnabledIT_TXE(dev_handle) && LL_USART_IsActiveFlag_TXE(dev_handle)) {
        uint8_t data = 0;

        if (!ctx->in_transmission) {
            if (ctx_io->handlers.tx_start) {
                ctx_io->handlers.tx_start(ctx_io, ctx_io->handlers.tx_start_ctx);
            }
            ctx->in_transmission = true;
        }

        if (xStreamBufferReceiveFromISR(ctx_io->buf_tx.handle, &data, 1U, &higher_task_woken)) {
            LL_USART_TransmitData8(dev_handle, data);
            ctx_io->stats.tx_transmited++;
        }
    }

    if (LL_USART_IsEnabledIT_TC(dev_handle) && LL_USART_IsActiveFlag_TC(dev_handle)) {
        if (ctx->in_transmission) {
            _uni_hal_usart_irq_tx_enable(ctx, false);
            ctx->in_transmission = false;

            if (ctx_io->handlers.tx_end) {
                ctx_io->handlers.tx_end(ctx_io, ctx_io->handlers.tx_end_ctx);
            }
        }
    }

    return higher_task_woken;
}

void LPUART1_IRQHandler(void) { portYIELD_FROM_ISR(_uni_hal_usart_irq_handler_lpuart(_uni_hal_lpuart_1_ctx)); }

void USART1_IRQHandler(void) { portYIELD_FROM_ISR(_uni_hal_usart_irq_handler(_uni_hal_usart_1_ctx)); }

void USART2_IRQHandler(void) { portYIELD_FROM_ISR(_uni_hal_usart_irq_handler(_uni_hal_usart_2_ctx)); }

void USART3_IRQHandler(void) { portYIELD_FROM_ISR(_uni_hal_usart_irq_handler(_uni_hal_usart_3_ctx)); }

void UART4_IRQHandler(void) { portYIELD_FROM_ISR(_uni_hal_usart_irq_handler(_uni_hal_uart_4_ctx)); }

void UART5_IRQHandler(void) { portYIELD_FROM_ISR(_uni_hal_usart_irq_handler(_uni_hal_uart_5_ctx)); }


//
// Public
//

bool uni_hal_usart_init(uni_hal_usart_context_t *ctx) {
    bool result = false;
    if (ctx != NULL && ctx->pin_rx != NULL && ctx->pin_tx != NULL) {
        // clock
        if (ctx->instance == UNI_HAL_CORE_PERIPH_LPUART_1)
        {
            uni_hal_rcc_clksrc_set(ctx->instance, UNI_HAL_RCC_CLKSRC_PCLK1);
        }
        result = uni_hal_rcc_clk_set(ctx->instance, true);

        // pins
        ctx->pin_rx->gpio_type = UNI_HAL_GPIO_TYPE_ALTERNATE_PP;
        ctx->pin_tx->gpio_type = UNI_HAL_GPIO_TYPE_ALTERNATE_PP;
        result = uni_hal_gpio_pin_init(ctx->pin_rx) && result;
        result = uni_hal_gpio_pin_init(ctx->pin_tx) && result;

        // enable IRQ
        result = _uni_hal_uart_nvic(ctx->instance, 0U) && result;
        result =_uni_hal_usart_irq_rx_enable(ctx, true) && result;

        void* handle = _uni_hal_uart_handle_get(ctx->instance);

        if(result && handle != NULL) {
            // init
            if (handle == LPUART1)
            {
                LL_LPUART_InitTypeDef LPUART_InitStruct = {0};
                LPUART_InitStruct.BaudRate = 115200;
                LPUART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
                LPUART_InitStruct.StopBits = LL_USART_STOPBITS_1;
                LPUART_InitStruct.Parity = LL_USART_PARITY_NONE;
                LPUART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
                LPUART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
                result = LL_LPUART_Init(handle, &LPUART_InitStruct) == SUCCESS;
            }
            else
            {
                LL_USART_InitTypeDef USART_InitStruct = {0};
                USART_InitStruct.BaudRate = 115200;
                USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
                USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
                USART_InitStruct.Parity = LL_USART_PARITY_NONE;
                USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
                USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
                USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
                result = LL_USART_Init(handle, &USART_InitStruct) == SUCCESS;
            }

            if(result) {
                // set handlers
                if (ctx->io) {
                    uni_hal_io_init(ctx->io);
                    ctx->io->handlers.tx_trigger = _uni_hal_usart_tx_trigger;
                    ctx->io->handlers.tx_trigger_ctx = ctx;
                }

                // save context
                _uni_hal_usart_save_ctx(ctx);

                // enable
                if (handle == LPUART1)
                {
                    LL_LPUART_Enable(handle);
                    while ((!(LL_LPUART_IsActiveFlag_TEACK(handle))) ||
                           (!(LL_LPUART_IsActiveFlag_REACK(handle)))) { }
                }
                else
                {
                    LL_USART_ConfigAsyncMode(handle);
                    LL_USART_Enable(handle);
                    while ((!(LL_USART_IsActiveFlag_TEACK(handle))) ||
                           (!(LL_USART_IsActiveFlag_REACK(handle)))) { }
                }
            }
        }

        ctx->initialized = result;
    }

    return result;
}


uint32_t uni_hal_usart_baudrate_get(uni_hal_usart_context_t *ctx) {
    uint32_t result = 0;

    if (uni_hal_uart_is_inited(ctx)) {
        void* instance = _uni_hal_uart_handle_get(ctx->instance);
        if (instance == LPUART1)
        {
            result = LL_LPUART_GetBaudRate(instance, uni_hal_rcc_clk_get_freq(ctx->instance));
        }
        else
        {
            result = LL_USART_GetBaudRate(instance, uni_hal_rcc_clk_get_freq(ctx->instance), LL_USART_GetOverSampling(instance));
        }
    }

    return result;
}


bool uni_hal_usart_baudrate_set(uni_hal_usart_context_t *ctx, uint32_t baudrate) {
    uint32_t result = 0;

    if (uni_hal_uart_is_inited(ctx)) {
        void* instance = _uni_hal_uart_handle_get(ctx->instance);
        if (instance != nullptr)
        {
            if (instance == LPUART1)
            {
                LL_LPUART_Disable(instance);
                LL_LPUART_SetBaudRate(instance, uni_hal_rcc_clk_get_freq(ctx->instance), baudrate);
                LL_LPUART_Enable(instance);

                while ((!(LL_LPUART_IsActiveFlag_TEACK(instance))) || (!(LL_LPUART_IsActiveFlag_REACK(instance)))) {}
            }
            else
            {
                LL_USART_Disable(instance);
                LL_USART_SetBaudRate(instance, uni_hal_rcc_clk_get_freq(ctx->instance),
                                     LL_USART_GetOverSampling(instance), baudrate);
                LL_USART_Enable(instance);

                while ((!(LL_USART_IsActiveFlag_TEACK(instance))) || (!(LL_USART_IsActiveFlag_REACK(instance)))) {}
            }

            result = true;
        }
    }

    return result;
}


bool uni_hal_usart_receive_enable(uni_hal_usart_context_t *ctx, bool value) {
    bool result = false;

    if (uni_hal_uart_is_inited(ctx)) {
        USART_TypeDef *instance = _uni_hal_uart_handle_get(ctx->instance);
        if (instance == LPUART1)
        {
            value ? LL_LPUART_EnableDirectionRx(instance) : LL_LPUART_DisableDirectionRx(instance);
        }
        else
        {
            value ? LL_USART_EnableDirectionRx(instance) : LL_USART_DisableDirectionRx(instance);
        }
        result = true;
    }

    return result;
}


bool uni_hal_usart_transmit_enable(uni_hal_usart_context_t *ctx, bool value)
{
    bool result = false;

    if (uni_hal_uart_is_inited(ctx))
    {
        void* instance = _uni_hal_uart_handle_get(ctx->instance);
        if (instance == LPUART1)
        {
            value ? LL_LPUART_EnableDirectionTx(instance) : LL_LPUART_DisableDirectionTx(instance);
        }
        else
        {
            value ? LL_USART_EnableDirectionTx(instance) : LL_USART_DisableDirectionTx(instance);
        }
        result = true;
    }

    return result;
}
