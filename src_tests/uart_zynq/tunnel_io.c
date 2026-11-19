// Use the embedded IO paths with real FreeRTOS stream buffers and the UART IRQ model.
#include "io/uni_hal_io.h"
#include "model.h"

static StreamBufferHandle_t wake_on_send;
static StreamBufferHandle_t wake_on_pop;

void tunnel_model_wake_on(StreamBufferHandle_t send, StreamBufferHandle_t pop) {
    wake_on_send = send;
    wake_on_pop = pop;
}

static size_t send_from_isr(StreamBufferHandle_t buf, const void *data, size_t len, BaseType_t *woken) {
    size_t const count = xStreamBufferSendFromISR(buf, data, len, woken);
    if (count && buf == wake_on_send && woken) {
        *woken = pdTRUE;
    }
    return count;
}

static size_t pop_from_isr(StreamBufferHandle_t buf, void *data, size_t len, BaseType_t *woken) {
    size_t const count = xStreamBufferReceiveFromISR(buf, data, len, woken);
    if (count && buf == wake_on_pop && woken) {
        *woken = pdTRUE;
    }
    return count;
}

#undef UNI_HAL_TARGET_MCU_PC
#undef taskENTER_CRITICAL
#undef taskEXIT_CRITICAL
#undef portYIELD_FROM_ISR
#define xPortIsInsideInterrupt uart_model_in_isr
#define taskENTER_CRITICAL() uart_model_task_enter()
#define taskEXIT_CRITICAL() uart_model_task_exit()
#define portYIELD_FROM_ISR(woken) uart_model_yield(woken)
#define xStreamBufferSendFromISR send_from_isr
#define xStreamBufferReceiveFromISR pop_from_isr
#include "io/uni_hal_io_c.c"
