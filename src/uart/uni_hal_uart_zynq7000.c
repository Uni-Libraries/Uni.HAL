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
// If the FIFO drains before the last stop bit, the global timer comparator checks for wire idle.
// RX: FIFO trigger level / timeout interrupts move the bytes into the RX buffer.
//

//
// Includes
//

// stdlib
#include <stddef.h>

// Xilinx BSP
#include <xparameters_ps.h>
#include <xuartps.h>
#include <xiltimer.h>
#include <xil_io.h>

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


// CPU0's banked Cortex-A9 global timer comparator. The counter remains owned by the BSP.
#define GT_CONTROL (XPAR_GLOBAL_TMR_BASEADDR + 0x08U)
#define GT_STATUS  (XPAR_GLOBAL_TMR_BASEADDR + 0x0CU)
#define GT_CMP_LO  (XPAR_GLOBAL_TMR_BASEADDR + 0x10U)
#define GT_CMP_HI  (XPAR_GLOBAL_TMR_BASEADDR + 0x14U)
#define GT_ENABLE       (1U << 0)
#define GT_COMPARE      (1U << 1)
#define GT_IRQ          (1U << 2)
#define GT_AUTOINCREMENT (1U << 3)

static uni_hal_usart_context_t *draining[UNI_HAL_UART_ZYNQ7000_COUNT];
static bool alarm_connected, alarm_armed;
static uint64_t alarm_deadline;

static bool alarm_available(void) {
    uint32_t const control = Xil_In32(GT_CONTROL);
    return (control & (GT_ENABLE | GT_COMPARE | GT_IRQ | GT_AUTOINCREMENT)) == GT_ENABLE &&
           !(Xil_In32(GT_STATUS) & 1U) && uni_hal_core_zynq7000_global_timer_irq_available();
}

// UARTs share one alarm. An earlier deadline can advance an existing check for the other port.
static void schedule_check(unsigned port) {
    XTime now;
    XTime_GetTime(&now);
    uint32_t const control = Xil_In32(GT_CONTROL);
    uint32_t const prescaler = 1U + ((control >> 8) & 0xFFU);
    uint32_t const baud = _uni_hal_uart_zynq7000_dev[port].BaudRate;
    // At TXEMPTY there is at most one 8N1 character on the wire. TACTIVE decides completion.
    uint64_t const counts = 10ULL * COUNTS_PER_SECOND / prescaler;
    uint64_t const delay = (counts + baud - 1U) / baud;
    uint64_t const deadline = now + delay;
    if (alarm_armed && alarm_deadline <= deadline) {
        return;
    }
    uint32_t const disabled = control & ~(GT_COMPARE | GT_IRQ);
    Xil_Out32(GT_CONTROL, disabled);
    Xil_Out32(GT_CMP_LO, (uint32_t)deadline);
    Xil_Out32(GT_CMP_HI, (uint32_t)(deadline >> 32));
    Xil_Out32(GT_STATUS, 1U);
    alarm_deadline = deadline;
    alarm_armed = true;
    // Zynq-7000 uses Cortex-A9 r3p0: count >= comparator also fires a deadline passed during setup.
    Xil_Out32(GT_CONTROL, disabled | GT_COMPARE | GT_IRQ);
}


/* moves bytes from the TX buffer into the TX FIFO; must not race with itself (ISR or masked) */
static bool _uni_hal_uart_zynq7000_fill_fifo(uni_hal_usart_context_t *ctx, uint32_t base, BaseType_t *woken) {
    bool any = false;
    uint8_t data = 0U;
    while ((XUartPs_ReadReg(base, XUARTPS_SR_OFFSET) & XUARTPS_SR_TXFULL) == 0U) {
        size_t count = woken ? uni_hal_io_buffer_pop_isr(&ctx->io->buf_tx, &data, 1U, woken) :
                               xStreamBufferReceive(ctx->io->buf_tx.handle, &data, 1U, 0U);
        if (count == 0U) {
            break;
        }
        if (!ctx->in_transmission) {
            ctx->in_transmission = true;
            if (ctx->io->handlers.tx_start) {
                ctx->io->handlers.tx_start(ctx->io, ctx->io->handlers.tx_start_ctx);
            }
        }
        XUartPs_WriteReg(base, XUARTPS_FIFO_OFFSET, data);
        ctx->io->stats.tx_transmited++;
        any = true;
    }
    return any;
}



//
// IO handlers
//

static void _uni_hal_uart_zynq7000_start_tx(uni_hal_usart_context_t *ctx, uint32_t base, BaseType_t *woken) {
    unsigned const port = (unsigned)_uni_hal_uart_zynq7000_index(ctx->instance);
    if ((!ctx->in_transmission || draining[port] != NULL) && _uni_hal_uart_zynq7000_fill_fifo(ctx, base, woken)) {
        draining[port] = NULL;
        XUartPs_WriteReg(base, XUARTPS_IER_OFFSET, XUARTPS_IXR_TXEMPTY);
    }
}

static void _uni_hal_uart_zynq7000_tx_trigger(void *ctx_io_r, void *ctx_fn_r) {
    (void)ctx_io_r;
    uni_hal_usart_context_t *ctx = ctx_fn_r;
    if (ctx == NULL || !ctx->initialized) {
        return;
    }

    uint32_t const base = _uni_hal_uart_zynq7000_base(ctx->instance);
    if (xPortIsInsideInterrupt()) {
        BaseType_t woken = pdFALSE;
        UBaseType_t const mask = taskENTER_CRITICAL_FROM_ISR();
        _uni_hal_uart_zynq7000_start_tx(ctx, base, &woken);
        taskEXIT_CRITICAL_FROM_ISR(mask);
        // The Cortex-A9 port retains this request until the outermost interrupt returns.
        if (woken) {
            portYIELD_FROM_ISR(pdTRUE);
        }
    } else {
        taskENTER_CRITICAL();
        _uni_hal_uart_zynq7000_start_tx(ctx, base, NULL);
        taskEXIT_CRITICAL();
    }
}



//
// IRQ handler
//

static void finish_or_defer(uni_hal_usart_context_t *ctx, uint32_t base, BaseType_t *woken) {
    unsigned const port = (unsigned)_uni_hal_uart_zynq7000_index(ctx->instance);
    uint32_t const status = XUartPs_ReadReg(base, XUARTPS_SR_OFFSET);
    if (!(status & XUARTPS_SR_TXEMPTY)) {
        // A stale empty event after a refill: wait for the fresh FIFO interrupt.
        draining[port] = NULL;
        XUartPs_WriteReg(base, XUARTPS_IER_OFFSET, XUARTPS_IXR_TXEMPTY);
        return;
    }
    XUartPs_WriteReg(base, XUARTPS_IDR_OFFSET, XUARTPS_IXR_TXEMPTY);
    if (status & XUARTPS_SR_TACTIVE) {
        draining[port] = ctx;
        schedule_check(port);
    } else {
        draining[port] = NULL;
        ctx->in_transmission = false;
        if (ctx->io->handlers.tx_end) {
            ctx->io->handlers.tx_end(ctx->io, ctx->io->handlers.tx_end_ctx);
        }
        if (ctx->callback && ctx->callback(ctx, ctx->callback_cookie, UNI_HAL_USART_CALLBACK_TC)) {
            *woken = pdTRUE;
        }
    }
}

static void check_shift_registers(void *unused) {
    (void)unused;
    BaseType_t woken = pdFALSE;
    UBaseType_t const mask = taskENTER_CRITICAL_FROM_ISR();
    if (Xil_In32(GT_STATUS) & 1U) {
        Xil_Out32(GT_CONTROL, Xil_In32(GT_CONTROL) & ~(GT_COMPARE | GT_IRQ));
        Xil_Out32(GT_STATUS, 1U);
        alarm_armed = false;
        for (unsigned port = 0; port < UNI_HAL_UART_ZYNQ7000_COUNT; ++port) {
            uni_hal_usart_context_t *ctx = draining[port];
            if (ctx != NULL) {
                uint32_t const base = _uni_hal_uart_zynq7000_base(ctx->instance);
                // A producer may already have queued bytes before its trigger runs.
                _uni_hal_uart_zynq7000_start_tx(ctx, base, &woken);
                if (draining[port] != NULL) {
                    finish_or_defer(ctx, base, &woken);
                }
            }
        }
    }
    taskEXIT_CRITICAL_FROM_ISR(mask);
    portYIELD_FROM_ISR(woken);
}

static void _uni_hal_uart_zynq7000_irq_handler(void *arg) {
    uni_hal_usart_context_t *ctx = (uni_hal_usart_context_t *)arg;
    if (ctx == NULL || ctx->io == NULL) {
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
        UBaseType_t const mask = taskENTER_CRITICAL_FROM_ISR();
        if (ctx->in_transmission && !_uni_hal_uart_zynq7000_fill_fifo(ctx, base, &woken)) {
            finish_or_defer(ctx, base, &woken);
        }
        taskEXIT_CRITICAL_FROM_ISR(mask);
    }

    portYIELD_FROM_ISR(woken);
}



//
// Public
//

bool uni_hal_usart_init(uni_hal_usart_context_t *ctx) {
    bool result = false;
    if (ctx != NULL && ctx->io != NULL) {
        // Both the UART vector and the shared TX alarm call FreeRTOS FromISR functions.
        uint32_t const default_prio = UNI_HAL_CORE_ZYNQ7000_IRQ_PRIORITY_DEFAULT < configMAX_API_CALL_INTERRUPT_PRIORITY
            ? configMAX_API_CALL_INTERRUPT_PRIORITY : UNI_HAL_CORE_ZYNQ7000_IRQ_PRIORITY_DEFAULT;
        uint32_t const prio = ctx->isr_priority ? ctx->isr_priority : default_prio;
        if (prio < configMAX_API_CALL_INTERRUPT_PRIORITY || prio > portLOWEST_USABLE_INTERRUPT_PRIORITY) {
            return false;
        }
        int32_t idx = _uni_hal_uart_zynq7000_index(ctx->instance);
        if (idx >= 0 && (alarm_connected || alarm_available())) {
            XUartPs *dev = &_uni_hal_uart_zynq7000_dev[idx];
            uint32_t base = _uni_hal_uart_zynq7000_base(ctx->instance);

            ctx->callback = NULL;
            ctx->callback_cookie = NULL;
            ctx->in_transmission = false;
            ctx->initialized = false;

            XUartPs_Config *cfg = XUartPs_LookupConfig(base);
            result = cfg != NULL && XUartPs_CfgInitialize(dev, cfg, cfg->BaseAddress) == XST_SUCCESS;
            if (result && ctx->baudrate != 0U) {
                result = XUartPs_SetBaudRate(dev, ctx->baudrate) == XST_SUCCESS;
            }
            if (result) {
                XUartPs_WriteReg(base, XUARTPS_IDR_OFFSET, XUARTPS_IXR_MASK);
                XUartPs_WriteReg(base, XUARTPS_ISR_OFFSET, XUARTPS_IXR_MASK);
                XUartPs_SetFifoThreshold(dev, UNI_HAL_UART_ZYNQ7000_RX_TRIGGER);
                XUartPs_SetRecvTimeout(dev, UNI_HAL_UART_ZYNQ7000_RX_TIMEOUT);

                result = uni_hal_io_init(ctx->io);
            }
            if (result) {
                uint32_t const mask = uni_hal_core_irq_pause();
                bool const had_alarm = alarm_connected;
                if (!had_alarm) {
                    alarm_connected = alarm_available() && uni_hal_core_zynq7000_irq_connect(
                        XPS_GLOBAL_TMR_INT_ID, check_shift_registers, NULL, default_prio, true);
                }
                // Publish the caller's pointer to the UART vector only when everything else is ready.
                result = alarm_connected && uni_hal_core_zynq7000_irq_connect(
                    _uni_hal_uart_zynq7000_irq(ctx->instance), _uni_hal_uart_zynq7000_irq_handler, ctx, prio, false);
                if (!result && !had_alarm && alarm_connected) {
                    XScuGic_Disconnect(&g_uni_hal_core_zynq7000_gic, XPS_GLOBAL_TMR_INT_ID);
                    alarm_connected = false;
                }
                uni_hal_core_irq_resume(mask);
                if (!result) {
                    uni_hal_io_deinit(ctx->io);
                }
            }
            if (result) {
                draining[idx] = NULL;
                ctx->io->handlers.tx_trigger = _uni_hal_uart_zynq7000_tx_trigger;
                ctx->io->handlers.tx_trigger_ctx = ctx;
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
