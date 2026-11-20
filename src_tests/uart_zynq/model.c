#include <stddef.h>

#include "model.h"
#include "xuartps.h"
#include "xparameters_ps.h"
#include "xiltimer.h"
#include "xil_io.h"
#include "zynq_core.h"

static struct {
    uint8_t tx[256], rx_byte;
    unsigned sent, queued;
    uint32_t mask, status;
    bool shifting, rx_ready;
} ports[2];
static XUartPs_Config cfgs[] = {{XPS_UART0_BASEADDR}, {XPS_UART1_BASEADDR}};
static struct { Xil_InterruptHandler handler; void *ctx; unsigned priority; } vectors[96];
static bool in_isr, event;
static uint64_t clock_count, comparator;
bool uart_model_ppi_available = true;
unsigned uart_model_fail_irq;
XScuGic g_uni_hal_core_zynq7000_gic;
bool uart_model_irq_connected(unsigned id) { return vectors[id].handler != NULL; }
unsigned uart_model_irq_priority(unsigned id) { return vectors[id].priority; }
void XScuGic_Disconnect(XScuGic *gic, uint32_t id) {
    (void)gic;
    vectors[id].handler = NULL;
    vectors[id].ctx = NULL;
}
uint32_t uart_model_timer_control = 1U;
unsigned uart_model_bad_task_critical, uart_model_yields;

unsigned uart_model_tx_count(unsigned port) { return ports[port].sent; }
uint8_t uart_model_tx_byte(unsigned port, unsigned index) { return ports[port].tx[index]; }
bool uart_model_in_isr(void) { return in_isr; }
void uart_model_task_enter(void) { if (in_isr) ++uart_model_bad_task_critical; }
void uart_model_task_exit(void) {}
void uart_model_yield(BaseType_t wake) { if (wake) ++uart_model_yields; }

XUartPs_Config *XUartPs_LookupConfig(uint32_t base) { return &cfgs[base == XPS_UART1_BASEADDR]; }
int XUartPs_CfgInitialize(XUartPs *dev, XUartPs_Config *cfg, uint32_t base) {
    (void)cfg; (void)base; dev->BaudRate = 115200U; return XST_SUCCESS;
}
int XUartPs_SetBaudRate(XUartPs *dev, uint32_t baudrate) { dev->BaudRate = baudrate; return XST_SUCCESS; }
void XUartPs_SetFifoThreshold(XUartPs *dev, uint8_t value) { (void)dev; (void)value; }
void XUartPs_SetRecvTimeout(XUartPs *dev, uint8_t value) { (void)dev; (void)value; }

uint32_t XUartPs_ReadReg(uint32_t address, uint32_t offset) {
    unsigned p = address == XPS_UART1_BASEADDR;
    switch (offset) {
    case XUARTPS_IMR_OFFSET: return ports[p].mask;
    case XUARTPS_ISR_OFFSET: return ports[p].status;
    case XUARTPS_SR_OFFSET:
        return (ports[p].queued == 0U ? XUARTPS_SR_TXEMPTY : 0U) | (ports[p].queued == 64U ? XUARTPS_SR_TXFULL : 0U) |
               (ports[p].shifting ? XUARTPS_SR_TACTIVE : 0U) | (ports[p].rx_ready ? 0U : XUARTPS_SR_RXEMPTY);
    case XUARTPS_FIFO_OFFSET: ports[p].rx_ready = false; return ports[p].rx_byte;
    default: return 0U;
    }
}
void XUartPs_WriteReg(uint32_t address, uint32_t offset, uint32_t value) {
    unsigned p = address == XPS_UART1_BASEADDR;
    switch (offset) {
    case XUARTPS_IER_OFFSET: ports[p].mask |= value; break;
    case XUARTPS_IDR_OFFSET: ports[p].mask &= ~value; break;
    case XUARTPS_ISR_OFFSET: ports[p].status &= ~value; break;
    case XUARTPS_FIFO_OFFSET:
        ports[p].tx[ports[p].sent++] = (uint8_t)value;
        ++ports[p].queued;
        ports[p].shifting = true;
        break;
    default: break;
    }
}
void uart_model_drain(unsigned p, bool shifting) {
    ports[p].queued = 0;
    ports[p].shifting = shifting;
    ports[p].status |= XUARTPS_IXR_TXEMPTY;
}
void uart_model_finish(unsigned p) { ports[p].shifting = false; }
void uart_model_rx(unsigned p, uint8_t byte) {
    ports[p].rx_ready = true;
    ports[p].rx_byte = byte;
    ports[p].status |= XUARTPS_IXR_TOUT;
}
bool uni_hal_core_zynq7000_irq_connect(uint32_t id, Xil_InterruptHandler handler, void *ctx,
                                       uint32_t priority, bool edge) {
    (void)edge;
    if (id == uart_model_fail_irq) return false;
    vectors[id].handler = handler;
    vectors[id].ctx = ctx;
    vectors[id].priority = priority;
    return true;
}
bool uni_hal_core_zynq7000_global_timer_irq_available(void) { return uart_model_ppi_available; }
static void dispatch(unsigned id) {
    bool outer = in_isr;
    in_isr = true;
    vectors[id].handler(vectors[id].ctx);
    in_isr = outer;
}
void uart_model_irq(unsigned p) {
    if (ports[p].mask & ports[p].status) dispatch(p == 0U ? XPS_UART0_INT_ID : XPS_UART1_INT_ID);
}

void XTime_GetTime(XTime *time) { *time = clock_count; }
uint32_t Xil_In32(uint32_t address) {
    return address == XPAR_GLOBAL_TMR_BASEADDR + 8U ? uart_model_timer_control : (uint32_t)event;
}
void Xil_Out32(uint32_t address, uint32_t value) {
    switch (address - XPAR_GLOBAL_TMR_BASEADDR) {
    case 8U: uart_model_timer_control = value; break;
    case 12U: if (value & 1U) event = false; break;
    case 16U: comparator = (comparator & 0xFFFFFFFF00000000ULL) | value; break;
    case 20U: comparator = ((uint64_t)value << 32) | (uint32_t)comparator; break;
    }
}
bool uart_model_alarm_armed(void) { return (uart_model_timer_control & 7U) == 7U; }
void uart_model_alarm(void) {
    if (uart_model_alarm_armed()) {
        clock_count = comparator;
        event = true;
        dispatch(XPS_GLOBAL_TMR_INT_ID);
    }
}
