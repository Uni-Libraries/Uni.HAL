#include <SEGGER_RTT.h>
#include "segger/uni_hal_segger_rtt.h"
#include "timer.h"

int main(void) {
    UNI_HAL_IO_DEFINITION(port, 8U, 8U);
    uni_hal_tim_context_t timer = {};
    uni_hal_segger_context_t ctx = {
        .io = &port_ctx, .timer = &timer, .buffer_index_up = 1U, .buffer_index_down = 1U,
    };
    char up[4], down[4], byte;
    CHECK(SEGGER_RTT_ConfigUpBuffer(1U, "up", up, sizeof up, SEGGER_RTT_MODE_NO_BLOCK_SKIP) == 0);
    CHECK(SEGGER_RTT_ConfigDownBuffer(1U, "down", down, sizeof down, SEGGER_RTT_MODE_NO_BLOCK_SKIP) == 0);
    CHECK(SEGGER_RTT_Write(1U, "u", 1U) == 1U);
    CHECK(SEGGER_RTT_WriteDownBuffer(1U, "d", 1U) == 1U);
    CHECK(!uni_hal_segger_rtt_is_inited());
    CHECK(uni_hal_segger_rtt_stdio_init(&ctx) && uni_hal_segger_rtt_is_inited());
    CHECK(SEGGER_RTT_HasDataUp(1U) && SEGGER_RTT_HasData(1U));
    CHECK(SEGGER_RTT_ReadUpBuffer(1U, &byte, 1U) == 1U && byte == 'u');
    CHECK(SEGGER_RTT_Read(1U, &byte, 1U) == 1U && byte == 'd');
    finish(0U);
}
