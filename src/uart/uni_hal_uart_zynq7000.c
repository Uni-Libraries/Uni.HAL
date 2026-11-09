//
// Zynq-7000 PS UART (Cadence UART, xuartps) driver.
//
// UNI_HAL_CORE_PERIPH_UART_1 = PS UART0 (0xE0000000), UNI_HAL_CORE_PERIPH_UART_2 = PS UART1
// (0xE0001000). The pins (MIO/EMIO) and the reference clock are set up by the PS7 configuration,
// pin_rx/pin_tx/clksrc of the context are not used. The console UART of the BSP is
// STDOUT_BASEADDRESS (xparameters.h).
//
// TX: the task side fills the FIFO under a critical section when the transmitter is idle and
// enables the TX-empty interrupt; the interrupt handler refills the FIFO from the TX buffer.
// RX: FIFO trigger level / timeout interrupts move the bytes into the RX buffer.
//

//
// Includes
//

// Xilinx BSP
#include <xparameters_ps.h>
#include <xuartps.h>

// FreeRTOS
#include <FreeRTOS.h>
#include <task.h>

// uni_hal
#include "core/uni_hal_core.h"
#include "io/uni_hal_io_tunnel.h"
#include "uart/uni_hal_uart.h"



//
// Defines
//

#define UNI_HAL_UART_ZYNQ7000_COUNT        (2U)
#define UNI_HAL_UART_ZYNQ7000_FIFO_SIZE    (64U)
#define UNI_HAL_UART_ZYNQ7000_RX_TRIGGER   (32U)  // RX FIFO level that raises an interrupt
#define UNI_HAL_UART_ZYNQ7000_RX_TIMEOUT   (8U)   // x4 bit periods of silence before the timeout interrupt
#define UNI_HAL_UART_ZYNQ7000_RX_IRQS      (XUARTPS_IXR_RXOVR | XUARTPS_IXR_TOUT | XUARTPS_IXR_RXFULL | XUARTPS_IXR_OVER)



//
// Private
//

static XUartPs _uni_hal_uart_zynq7000_dev[UNI_HAL_UART_ZYNQ7000_COUNT];


static int32_t _uni_hal_uart_zynq7000_index(uni_hal_core_periph_e instance) {
    switch (instance) {
        case UNI_HAL_CORE_PERIPH_UART_1: return 0;
        case UNI_HAL_CORE_PERIPH_UART_2: return 1;
        default:                         return -1;
    }
}


static uint32_t _uni_hal_uart_zynq7000_base(uni_hal_core_periph_e instance) {
    switch (instance) {
        case UNI_HAL_CORE_PERIPH_UART_1: return XPS_UART0_BASEADDR;
        case UNI_HAL_CORE_PERIPH_UART_2: return XPS_UART1_BASEADDR;
        default:                         return 0U;
    }
}


static uint32_t _uni_hal_uart_zynq7000_irq(uni_hal_core_periph_e instance) {
    switch (instance) {
        case UNI_HAL_CORE_PERIPH_UART_1: return XPS_UART0_INT_ID;
        case UNI_HAL_CORE_PERIPH_UART_2: return XPS_UART1_INT_ID;
        default:                         return 0U;
    }
}


/* moves bytes from the TX buffer into the TX FIFO; must not race with itself (ISR or masked) */
static bool _uni_hal_uart_zynq7000_fill_fifo(uni_hal_usart_context_t *ctx, uint32_t base, BaseType_t *woken) {
    bool any = false;
    uint8_t data = 0U;
    while ((XUartPs_ReadReg(base, XUARTPS_SR_OFFSET) & XUARTPS_SR_TXFULL) == 0U &&
           uni_hal_io_buffer_pop_isr(&ctx->io->buf_tx, &data, 1U, woken)) {
        XUartPs_WriteReg(base, XUARTPS_FIFO_OFFSET, data);
        ctx->io->stats.tx_transmited++;
        any = true;
    }
    return any;
}



//
// IO handlers
//

static void _uni_hal_uart_zynq7000_tx_trigger(void *ctx_io_r, void *ctx_fn_r) {
    (void)ctx_io_r;
    uni_hal_usart_context_t *ctx = (uni_hal_usart_context_t *)ctx_fn_r;
    if (ctx == nullptr || !ctx->initialized) {
        return;
    }

    uint32_t base = _uni_hal_uart_zynq7000_base(ctx->instance);
    BaseType_t woken = pdFALSE;

    // the UART interrupt (priority >= configMAX_API_CALL_INTERRUPT_PRIORITY) is masked here
    taskENTER_CRITICAL();
    if (!ctx->in_transmission) {
        if (ctx->io->handlers.tx_start) {
            ctx->io->handlers.tx_start(ctx->io, ctx->io->handlers.tx_start_ctx);
        }
        if (_uni_hal_uart_zynq7000_fill_fifo(ctx, base, &woken)) {
            ctx->in_transmission = true;
            // the TX-empty event fires when the FIFO drains
            XUartPs_WriteReg(base, XUARTPS_IER_OFFSET, XUARTPS_IXR_TXEMPTY);
        }
    }
    taskEXIT_CRITICAL();
}



//
// IRQ handler
//

static void _uni_hal_uart_zynq7000_irq_handler(void *arg) {
    uni_hal_usart_context_t *ctx = (uni_hal_usart_context_t *)arg;
    if (ctx == nullptr || ctx->io == nullptr) {
        return;
    }

    uni_hal_io_context_t *ctx_io = ctx->io;
    uint32_t base = _uni_hal_uart_zynq7000_base(ctx->instance);
    BaseType_t woken = pdFALSE;

    uint32_t isr = XUartPs_ReadReg(base, XUARTPS_ISR_OFFSET) & XUartPs_ReadReg(base, XUARTPS_IMR_OFFSET);
    XUartPs_WriteReg(base, XUARTPS_ISR_OFFSET, isr);

    // RX
    if ((isr & XUARTPS_IXR_OVER) != 0U) {
        ctx_io->stats.rx_overrun++;
    }
    if ((isr & UNI_HAL_UART_ZYNQ7000_RX_IRQS) != 0U) {
        while ((XUartPs_ReadReg(base, XUARTPS_SR_OFFSET) & XUARTPS_SR_RXEMPTY) == 0U) {
            uint8_t data = (uint8_t)XUartPs_ReadReg(base, XUARTPS_FIFO_OFFSET);

            uni_hal_io_tunnel_transmit_result_t tunnel_result = UNI_HAL_IO_TUNNEL_TRANSPARENT;
            if (ctx_io->handlers.tunnel_ctx) {
                tunnel_result = uni_hal_io_tunnel_transmit(ctx_io->handlers.tunnel_ctx, ctx_io, &data, sizeof(data));
            }
            if (tunnel_result == UNI_HAL_IO_TUNNEL_TRANSPARENT || tunnel_result == UNI_HAL_IO_TUNNEL_FAIL) {
                if (uni_hal_io_buffer_push_isr(&ctx_io->buf_rx, &data, 1U, &woken)) {
                    ctx_io->stats.rx_received++;
                }
                else {
                    ctx_io->stats.rx_overrun++;
                }
            }
        }
    }

    // TX: the FIFO has drained
    if ((isr & XUARTPS_IXR_TXEMPTY) != 0U) {
        if (!_uni_hal_uart_zynq7000_fill_fifo(ctx, base, &woken)) {
            // nothing left: the last bytes are in the shift register
            XUartPs_WriteReg(base, XUARTPS_IDR_OFFSET, XUARTPS_IXR_TXEMPTY);
            if (ctx->in_transmission) {
                ctx->in_transmission = false;
                if (ctx_io->handlers.tx_end) {
                    ctx_io->handlers.tx_end(ctx_io, ctx_io->handlers.tx_end_ctx);
                }
                if (ctx->callback && ctx->callback(ctx, ctx->callback_cookie, UNI_HAL_USART_CALLBACK_TC)) {
                    woken = pdTRUE;
                }
            }
        }
    }

    portYIELD_FROM_ISR(woken);
}



//
// Public
//

bool uni_hal_usart_init(uni_hal_usart_context_t *ctx) {
    bool result = false;
    if (ctx != nullptr && ctx->io != nullptr) {
        int32_t idx = _uni_hal_uart_zynq7000_index(ctx->instance);
        if (idx >= 0) {
            XUartPs *dev = &_uni_hal_uart_zynq7000_dev[idx];
            uint32_t base = _uni_hal_uart_zynq7000_base(ctx->instance);

            ctx->callback = nullptr;
            ctx->callback_cookie = nullptr;
            ctx->in_transmission = false;
            ctx->initialized = false;

            uni_hal_io_init(ctx->io);
            ctx->io->handlers.tx_trigger = _uni_hal_uart_zynq7000_tx_trigger;
            ctx->io->handlers.tx_trigger_ctx = ctx;

            XUartPs_Config *cfg = XUartPs_LookupConfig(base);
            result = cfg != nullptr && XUartPs_CfgInitialize(dev, cfg, cfg->BaseAddress) == XST_SUCCESS;
            if (result && ctx->baudrate != 0U) {
                result = XUartPs_SetBaudRate(dev, ctx->baudrate) == XST_SUCCESS;
            }
            if (result) {
                XUartPs_WriteReg(base, XUARTPS_IDR_OFFSET, XUARTPS_IXR_MASK);
                XUartPs_WriteReg(base, XUARTPS_ISR_OFFSET, XUARTPS_IXR_MASK);
                XUartPs_SetFifoThreshold(dev, UNI_HAL_UART_ZYNQ7000_RX_TRIGGER);
                XUartPs_SetRecvTimeout(dev, UNI_HAL_UART_ZYNQ7000_RX_TIMEOUT);

                uint32_t prio = ctx->isr_priority != 0U ? ctx->isr_priority : UNI_HAL_CORE_ZYNQ7000_IRQ_PRIORITY_DEFAULT;
                result = uni_hal_core_zynq7000_irq_connect(_uni_hal_uart_zynq7000_irq(ctx->instance),
                                                           _uni_hal_uart_zynq7000_irq_handler, ctx, prio, false);
            }
            if (result) {
                XUartPs_WriteReg(base, XUARTPS_IER_OFFSET, UNI_HAL_UART_ZYNQ7000_RX_IRQS);
                ctx->initialized = true;
            }
        }
    }
    return result;
}


uint32_t uni_hal_usart_baudrate_get(uni_hal_usart_context_t *ctx) {
    uint32_t result = 0U;
    if (uni_hal_uart_is_inited(ctx)) {
        result = _uni_hal_uart_zynq7000_dev[_uni_hal_uart_zynq7000_index(ctx->instance)].BaudRate;
    }
    return result;
}


bool uni_hal_usart_baudrate_set(uni_hal_usart_context_t *ctx, uint32_t baudrate) {
    bool result = false;
    if (uni_hal_uart_is_inited(ctx)) {
        XUartPs *dev = &_uni_hal_uart_zynq7000_dev[_uni_hal_uart_zynq7000_index(ctx->instance)];
        result = XUartPs_SetBaudRate(dev, baudrate) == XST_SUCCESS;
        if (result) {
            ctx->baudrate = baudrate;
        }
    }
    return result;
}


bool uni_hal_usart_receive_enable(uni_hal_usart_context_t *ctx, bool value) {
    bool result = false;
    if (uni_hal_uart_is_inited(ctx)) {
        uint32_t base = _uni_hal_uart_zynq7000_base(ctx->instance);
        uint32_t cr = XUartPs_ReadReg(base, XUARTPS_CR_OFFSET) & ~(XUARTPS_CR_RX_EN | XUARTPS_CR_RX_DIS);
        XUartPs_WriteReg(base, XUARTPS_CR_OFFSET, cr | (value ? XUARTPS_CR_RX_EN : XUARTPS_CR_RX_DIS));
        result = true;
    }
    return result;
}


bool uni_hal_usart_transmit_enable(uni_hal_usart_context_t *ctx, bool value) {
    bool result = false;
    if (uni_hal_uart_is_inited(ctx)) {
        uint32_t base = _uni_hal_uart_zynq7000_base(ctx->instance);
        uint32_t cr = XUartPs_ReadReg(base, XUARTPS_CR_OFFSET) & ~(XUARTPS_CR_TX_EN | XUARTPS_CR_TX_DIS);
        XUartPs_WriteReg(base, XUARTPS_CR_OFFSET, cr | (value ? XUARTPS_CR_TX_EN : XUARTPS_CR_TX_DIS));
        result = true;
    }
    return result;
}


bool uni_hal_usart_irq_enable(uni_hal_usart_context_t *ctx, bool value) {
    bool result = false;
    if (uni_hal_uart_is_inited(ctx)) {
        uint32_t base = _uni_hal_uart_zynq7000_base(ctx->instance);
        XUartPs_WriteReg(base, value ? XUARTPS_IER_OFFSET : XUARTPS_IDR_OFFSET, UNI_HAL_UART_ZYNQ7000_RX_IRQS);
        result = true;
    }
    return result;
}
