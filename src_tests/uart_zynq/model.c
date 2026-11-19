#include "model.h"
#include "xuartps.h"
#include "xparameters_ps.h"
#include "zynq_core.h"

static struct {
    uint8_t tx[256], rx_byte;
    unsigned sent, queued;
    uint32_t mask, status;
    bool shifting, rx_ready;
} ports[2];
static XUartPs_Config cfgs[] = {{XPS_UART0_BASEADDR}, {XPS_UART1_BASEADDR}};
static struct { Xil_InterruptHandler handler; void *ctx; } vectors[96];
static bool in_isr;
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
void uart_model_rx(unsigned p, uint8_t byte) {
    ports[p].rx_ready = true;
    ports[p].rx_byte = byte;
    ports[p].status |= XUARTPS_IXR_TOUT;
}
bool uni_hal_core_zynq7000_irq_connect(uint32_t id, Xil_InterruptHandler handler, void *ctx,
                                       uint32_t priority, bool edge) {
    (void)priority; (void)edge;
    vectors[id].handler = handler;
    vectors[id].ctx = ctx;
    return true;
}
static void dispatch(unsigned id) {
    bool outer = in_isr;
    in_isr = true;
    vectors[id].handler(vectors[id].ctx);
    in_isr = outer;
}
void uart_model_irq(unsigned p) {
    if (ports[p].mask & ports[p].status) dispatch(p == 0U ? XPS_UART0_INT_ID : XPS_UART1_INT_ID);
}
