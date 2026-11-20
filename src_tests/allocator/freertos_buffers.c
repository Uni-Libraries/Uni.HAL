#include <string.h>

#include "io/uni_hal_io.h"

// Run from a FreeRTOS task on the Cortex-M4 QEMU test board.
int uni_hal_test_io_buffers(void) {
    UNI_HAL_IO_DEFINITION(io, 5U, 7U);
    const uint8_t data[] = {1U, 2U, 3U, 4U, 5U, 6U, 7U};
    uint8_t output[sizeof(data)] = {0};
    const size_t initial_heap = xPortGetFreeHeapSize();

    if (!uni_hal_io_init(&io_ctx)) {
        return 10;
    }
    // A zero-byte read on an empty stream must leave the task's pending notification intact.
    (void)xTaskNotifyGive(xTaskGetCurrentTaskHandle());
    if (uni_hal_io_receive_data(&io_ctx, output, 0U, 8U) != 0U ||
        xTaskNotifyWait(0U, UINT32_MAX, nullptr, 0U) != pdTRUE) {
        return 18;
    }
    const size_t active_heap = xPortGetFreeHeapSize();
#if configSUPPORT_STATIC_ALLOCATION
    if (active_heap != initial_heap) {
        return 11;
    }
#else
    if (active_heap >= initial_heap) {
        return 11;
    }
#endif
    // Both allocation modes leave one slot free, including when a write would overflow.
    if (xStreamBufferSend(io_ctx.buf_rx.handle, data, sizeof(data), 0U) != 4U ||
        uni_hal_io_receive_available(&io_ctx) != 4U ||
        uni_hal_io_receive_data(&io_ctx, output, sizeof(output), 0U) != 4U ||
        memcmp(data, output, 4U) != 0) {
        return 12;
    }
    if (uni_hal_io_transmit_data(&io_ctx, data, sizeof(data)) != 6U ||
        xStreamBufferReceive(io_ctx.buf_tx.handle, output, sizeof(output), 0U) != 6U ||
        memcmp(data, output, 6U) != 0) {
        return 13;
    }
    if (uni_hal_io_transmit_data(&io_ctx, data, 1U) != 1U ||
        !uni_hal_io_init(&io_ctx) || !uni_hal_io_buffer_is_empty(&io_ctx.buf_tx) ||
        xPortGetFreeHeapSize() != active_heap) {
        return 14;
    }

#if !configSUPPORT_STATIC_ALLOCATION
    // On this 32-bit target the kernel rejects this size before heap allocation.
    // RX was already created, so the failed TX must release it and the old pair.
    io_ctx.buf_tx.size = UINT32_MAX;
    if (uni_hal_io_init(&io_ctx) || io_ctx.buf_rx.handle || io_ctx.buf_tx.handle ||
        xPortGetFreeHeapSize() != initial_heap) {
        return 15;
    }
    io_ctx.buf_tx.size = 3U;
    if (!uni_hal_io_init(&io_ctx) || uni_hal_io_transmit_data(&io_ctx, data, sizeof(data)) != 2U) {
        return 16;
    }
#endif
    uni_hal_io_deinit(&io_ctx);
    uni_hal_io_deinit(&io_ctx);
    if (io_ctx.buf_rx.handle || io_ctx.buf_tx.handle || xPortGetFreeHeapSize() != initial_heap) {
        return 17;
    }
    return 0;
}
