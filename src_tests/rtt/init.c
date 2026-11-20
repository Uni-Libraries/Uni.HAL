#include "timer.h"

int main(void) {
    UNI_HAL_IO_DEFINITION(port, 16U, 16U);
    uni_hal_tim_context_t timer = {};
    uni_hal_segger_context_t ctx = {.io = &port_ctx, .timer = &timer};

    port_ctx.buf_tx.size = 0U;
    CHECK(!uni_hal_segger_rtt_stdio_init(&ctx));
    CHECK(!uni_hal_segger_rtt_stdio_is_inited(&ctx) && init_calls == 0U);
    CHECK(!port_ctx.buf_rx.handle && !port_ctx.buf_tx.handle);
    port_ctx.buf_tx.size = 16U;
    size_t const free_before = xPortGetFreeHeapSize();

    for (fail_at = INIT_FAILURE; fail_at <= START_FAILURE; ++fail_at) {
        CHECK(!uni_hal_segger_rtt_stdio_init(&ctx));
        CHECK(!uni_hal_segger_rtt_stdio_is_inited(&ctx));
        CHECK(!running && !callback && !callback_ctx);
        CHECK(!port_ctx.buf_rx.handle && !port_ctx.buf_tx.handle);
        CHECK(xPortGetFreeHeapSize() == free_before);
    }
    CHECK(stop_calls == 1U);
    ctx.timer = nullptr;
    CHECK(!uni_hal_segger_rtt_stdio_init(&ctx) && init_calls == 3U);

    // A caller may already have an IO stream with data queued for transmission.
    ctx.timer = &timer;
    CHECK(uni_hal_io_init(&port_ctx));
    uint8_t byte = 'x';
    CHECK(uni_hal_io_transmit_data(&port_ctx, &byte, 1U) == 1U);
    fail_at = START_FAILURE;
    CHECK(!uni_hal_segger_rtt_stdio_init(&ctx));
    CHECK(port_ctx.buf_rx.handle && xStreamBufferBytesAvailable(port_ctx.buf_tx.handle) == 1U);
    fail_at = NO_FAILURE;
    CHECK(uni_hal_segger_rtt_stdio_init(&ctx) && uni_hal_segger_rtt_stdio_is_inited(&ctx));
    CHECK(running && callback && callback_ctx == &ctx);
    CHECK(!uni_hal_segger_rtt_stdio_init(&ctx));
    CHECK(xStreamBufferBytesAvailable(port_ctx.buf_tx.handle) == 1U);
    finish(0U);
}
