#include <SEGGER_RTT.h>
#include "segger/uni_hal_segger_rtt.h"
#include "timer.h"

static void expect_unused(uni_hal_segger_context_t *ctx, uint32_t up, uint32_t down) {
    ctx->buffer_index_up = up;
    ctx->buffer_index_down = down;
    CHECK(!uni_hal_segger_rtt_stdio_init(ctx));
    CHECK(!ctx->initialized && init_calls == 0U && !callback);
    CHECK(!ctx->io->buf_rx.handle && !ctx->io->buf_tx.handle);
}

int main(void) {
    UNI_HAL_IO_DEFINITION(port, 8U, 8U);
    uni_hal_tim_context_t timer = {};
    uni_hal_segger_context_t ctx = {.io = &port_ctx, .timer = &timer};
    expect_unused(&ctx, SEGGER_RTT_MAX_NUM_UP_BUFFERS, 0U);
    expect_unused(&ctx, 0U, SEGGER_RTT_MAX_NUM_DOWN_BUFFERS);
    CHECK(!uni_hal_segger_rtt_is_inited());

    uni_hal_segger_rtt_init();
    expect_unused(&ctx, 1U, 0U);
    expect_unused(&ctx, 0U, 1U);
    char up[2], down[2];
    CHECK(SEGGER_RTT_ConfigUpBuffer(1U, "up", up, 1U, SEGGER_RTT_MODE_NO_BLOCK_SKIP) == 0);
    expect_unused(&ctx, 1U, 0U);
    CHECK(SEGGER_RTT_ConfigUpBuffer(1U, "up", up, sizeof up, SEGGER_RTT_MODE_NO_BLOCK_SKIP) == 0);
    CHECK(SEGGER_RTT_ConfigDownBuffer(1U, "down", down, 1U, SEGGER_RTT_MODE_NO_BLOCK_SKIP) == 0);
    expect_unused(&ctx, 1U, 1U);
    CHECK(SEGGER_RTT_ConfigDownBuffer(1U, "down", down, sizeof down, SEGGER_RTT_MODE_NO_BLOCK_SKIP) == 0);
    CHECK(uni_hal_segger_rtt_stdio_init(&ctx) && running);
    finish(0U);
}
