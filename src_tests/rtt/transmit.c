#include <string.h>
#include <SEGGER_RTT.h>
#include "segger/uni_hal_segger_rtt.h"
#include "timer.h"

static void test_tx(void *unused) {
    (void)unused;
    UNI_HAL_IO_DEFINITION(port, 8U, 8U);
    uni_hal_tim_context_t timer = {};
    uni_hal_segger_context_t ctx = {.io = &port_ctx, .timer = &timer, .buffer_index_up = 1U};
    char ring[4]; // Three usable slots.
    char received[3];
    uint8_t const data[] = {'a', 0, 0xff, 'd', 'e'};

    uni_hal_segger_rtt_init();
    CHECK(SEGGER_RTT_ConfigUpBuffer(1U, "test", ring, sizeof ring, SEGGER_RTT_MODE_NO_BLOCK_SKIP) == 0);
    CHECK(uni_hal_segger_rtt_stdio_init(&ctx));

    CHECK(uni_hal_io_transmit_data(&port_ctx, data, sizeof data) == sizeof data);
    poll_from_irq();
    CHECK(port_ctx.stats.tx_transmited == 3U && port_ctx.stats.tx_dropped == 2U);
    CHECK(uni_hal_io_buffer_is_empty(&port_ctx.buf_tx));
    CHECK(SEGGER_RTT_ReadUpBuffer(1U, received, sizeof received) == sizeof received);
    CHECK(memcmp(received, data, sizeof received) == 0);

    CHECK(SEGGER_RTT_SetFlagsUpBuffer(1U, SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL) == 0);
    CHECK(SEGGER_RTT_Write(1U, "xxx", 3U) == 3U);
    CHECK(uni_hal_io_transmit_data(&port_ctx, data, 1U) == 1U);
    poll_from_irq();
    CHECK(port_ctx.stats.tx_transmited == 3U && port_ctx.stats.tx_dropped == 3U);
    CHECK(SEGGER_RTT_ReadUpBuffer(1U, received, 3U) == 3U && memcmp(received, "xxx", 3U) == 0);
    CHECK(uni_hal_io_transmit_data(&port_ctx, data + 3, 2U) == 2U);
    poll_from_irq();
    CHECK(port_ctx.stats.tx_transmited == 5U && port_ctx.stats.tx_dropped == 3U);
    CHECK(SEGGER_RTT_ReadUpBuffer(1U, received, 2U) == 2U && memcmp(received, data + 3, 2U) == 0);
    finish(0U);
}

int main(void) {
    CHECK(xTaskCreate(test_tx, "rtt", 512U, nullptr, 1U, nullptr) == pdPASS);
    vTaskStartScheduler();
    finish(__LINE__);
}
