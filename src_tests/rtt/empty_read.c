#include <SEGGER_RTT.h>
#include "timer.h"

static unsigned reads;
static char other_byte;

unsigned __real_SEGGER_RTT_Read(unsigned index, void *buffer, unsigned size);
unsigned __wrap_SEGGER_RTT_Read(unsigned index, void *buffer, unsigned size) {
    if (++reads == 2U) {
        // Another reader runs before the poller's second read acquires SEGGER's lock.
        CHECK(__real_SEGGER_RTT_Read(index, &other_byte, 1U) == 1U);
    }
    return __real_SEGGER_RTT_Read(index, buffer, size);
}

int main(void) {
    UNI_HAL_IO_DEFINITION(port, 8U, 8U);
    uni_hal_tim_context_t timer = {};
    uni_hal_segger_context_t ctx = {.io = &port_ctx, .timer = &timer};
    CHECK(uni_hal_segger_rtt_stdio_init(&ctx));
    CHECK(SEGGER_RTT_WriteDownBuffer(0U, "xy", 2U) == 2U);
    callback(nullptr, callback_ctx);
    CHECK(other_byte == 'y');
    CHECK(port_ctx.stats.rx_received == 1U && port_ctx.stats.rx_overrun == 0U);
    uint8_t bytes[2];
    CHECK(uni_hal_io_receive_data(&port_ctx, bytes, sizeof bytes, 0U) == 1U && bytes[0] == 'x');
    finish(0U);
}
