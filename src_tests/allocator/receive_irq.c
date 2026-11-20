#include <stm32l4xx.h>
#include "io/uni_hal_io.h"

UNI_HAL_IO_DEFINITION(irq_rx, 8, 2);
static const uint8_t marker[] = {'A', 'B', 'C'};
static uint8_t received[2];
static volatile bool search;
static volatile int result;

void PVD_PVM_IRQHandler(void) {
    if (search) {
        result = uni_hal_io_receive_sync(&irq_rx_ctx, marker, sizeof(marker), 20U);
    } else {
        result = (int)uni_hal_io_receive_data(&irq_rx_ctx, received, sizeof(received), 20U);
    }
}

static int poll_from_irq(bool marker_search) {
    search = marker_search;
    result = -1;
    NVIC_SetPendingIRQ(PVD_PVM_IRQn);
    __DSB();
    __ISB();
    return result;
}

int uni_hal_test_receive_irq(void) {
    if (!uni_hal_io_init(&irq_rx_ctx)) {
        return __LINE__;
    }
    NVIC_SetPriority(PVD_PVM_IRQn, configMAX_SYSCALL_INTERRUPT_PRIORITY >> (8U - __NVIC_PRIO_BITS));
    NVIC_EnableIRQ(PVD_PVM_IRQn);

    // This IRQ masks the scheduler tick. A positive timeout must never spin for another byte.
    if (poll_from_irq(false) != 0 || poll_from_irq(true) != 0 ||
        xStreamBufferSend(irq_rx_ctx.buf_rx.handle, "AB", 2, 0) != 2U ||
        poll_from_irq(true) != 0 ||
        xStreamBufferSend(irq_rx_ctx.buf_rx.handle, "C!", 2, 0) != 2U ||
        poll_from_irq(true) != 1 || poll_from_irq(false) != 1 || received[0] != '!') {
        return __LINE__;
    }
    NVIC_DisableIRQ(PVD_PVM_IRQn);
    uni_hal_io_deinit(&irq_rx_ctx);
    return 0;
}
