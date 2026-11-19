#include <stdio.h>
#include <string.h>

#include "io/uni_hal_io.h"
#include "systick/uni_hal_systick.h"

static uint32_t now_ms;
static unsigned reads;
static unsigned refill_bytes;
static bool waiting_for_byte;
static TickType_t last_wait;
static uint32_t wake_after_ms;

static BaseType_t scheduler_state(void) {
    return waiting_for_byte ? taskSCHEDULER_RUNNING : xTaskGetSchedulerState();
}

uint32_t uni_hal_systick_get_ms(void) {
    return now_ms;
}

// Use a real FreeRTOS stream buffer. Advance a controlled clock and model the
// producer immediately refilling each slot freed by the consumer.
static size_t receive_byte(StreamBufferHandle_t buffer, void *data, size_t size, TickType_t wait) {
    if (waiting_for_byte) {
        last_wait = wait;
        now_ms += wake_after_ms;
        waiting_for_byte = false;
        uint8_t final_byte = 'B';
        (void)xStreamBufferSend(buffer, &final_byte, 1U, 0);
    } else {
        ++now_ms;
    }
    size_t count = xStreamBufferReceive(buffer, data, size, 0);
    if (count != 0U) {
        ++reads;
        if (refill_bytes != 0U) {
            --refill_bytes;
            uint8_t byte = 'A';
            (void)xStreamBufferSend(buffer, &byte, 1U, 0);
        }
    }
    return count;
}

#define xTaskGetSchedulerState scheduler_state
#define xStreamBufferReceive receive_byte
#include "io/uni_hal_io_c.c"
#undef xStreamBufferReceive
#undef xTaskGetSchedulerState

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "line %d: %s\n", __LINE__, #expr); return false; } } while (0)

static bool send_bytes(uni_hal_io_context_t *ctx, const char *bytes) {
    size_t size = strlen(bytes);
    return xStreamBufferSend(ctx->buf_rx.handle, bytes, size, 0) == size;
}

static bool changed_pattern(void) {
    UNI_HAL_IO_DEFINITION(io, 12, 3);
    const uint8_t first[] = "ABX";
    const uint8_t next[] = "QQZ";
    CHECK(uni_hal_io_init(&io_ctx));
    CHECK(send_bytes(&io_ctx, "AB"));
    CHECK(!uni_hal_io_receive_sync(&io_ctx, first, 3, 0));
    CHECK(send_bytes(&io_ctx, "Z"));
    CHECK(!uni_hal_io_receive_sync(&io_ctx, next, 3, 0));
    CHECK(send_bytes(&io_ctx, "AB"));
    CHECK(!uni_hal_io_receive_sync(&io_ctx, first, 3, 0));
    CHECK(send_bytes(&io_ctx, "A"));
    CHECK(uni_hal_io_receive_sync(&io_ctx, first, 1, 0));
    uni_hal_io_deinit(&io_ctx);
    return true;
}

static bool overlapping_fragments(void) {
    UNI_HAL_IO_DEFINITION(io, 12, 3);
    const uint8_t marker[] = "ABABC";
    CHECK(uni_hal_io_init(&io_ctx));
    CHECK(send_bytes(&io_ctx, "ABAB"));
    CHECK(!uni_hal_io_receive_sync(&io_ctx, marker, 5, 0));
    CHECK(!uni_hal_io_receive_sync(&io_ctx, marker, 5, 0));
    CHECK(send_bytes(&io_ctx, "ABC!"));
    CHECK(uni_hal_io_receive_sync(&io_ctx, marker, 5, 0));
    CHECK(uni_hal_io_receive_available(&io_ctx) == 1U);
    uni_hal_io_deinit(&io_ctx);
    return true;
}

static bool reusable_marker(void) {
    UNI_HAL_IO_DEFINITION(io, 12, 3);
    uint8_t marker[] = "ABX";
    CHECK(uni_hal_io_init(&io_ctx));
    CHECK(send_bytes(&io_ctx, "AB"));
    CHECK(!uni_hal_io_receive_sync(&io_ctx, marker, 3, 0));
    CHECK(send_bytes(&io_ctx, "Z"));
    uni_hal_io_receive_sync_reset(&io_ctx);
    memcpy(marker, "QQZ", 3);
    CHECK(uni_hal_io_receive_available(&io_ctx) == 1U);
    CHECK(!uni_hal_io_receive_sync(&io_ctx, marker, 3, 0));
    CHECK(send_bytes(&io_ctx, "QQZ"));
    CHECK(uni_hal_io_receive_sync(&io_ctx, marker, 3, 0));

    CHECK(send_bytes(&io_ctx, "QQ"));
    CHECK(!uni_hal_io_receive_sync(&io_ctx, marker, 3, 0));
    CHECK(uni_hal_io_receive_clear(&io_ctx));
    CHECK(send_bytes(&io_ctx, "Z"));
    CHECK(!uni_hal_io_receive_sync(&io_ctx, marker, 3, 0));
    CHECK(send_bytes(&io_ctx, "QQ"));
    CHECK(!uni_hal_io_receive_sync(&io_ctx, marker, 3, 0));
    CHECK(uni_hal_io_init(&io_ctx));
    CHECK(send_bytes(&io_ctx, "Z"));
    CHECK(!uni_hal_io_receive_sync(&io_ctx, marker, 3, 0));
    uni_hal_io_deinit(&io_ctx);
    return true;
}

static bool another_read(bool from_isr) {
    UNI_HAL_IO_DEFINITION(io, 12, 3);
    const uint8_t marker[] = "ABC";
    uint8_t byte = 0U;
    CHECK(uni_hal_io_init(&io_ctx));
    CHECK(send_bytes(&io_ctx, "AB"));
    CHECK(!uni_hal_io_receive_sync(&io_ctx, marker, 3, 0));
    CHECK(send_bytes(&io_ctx, "XC"));
    if (from_isr) {
        BaseType_t woken = pdFALSE;
        CHECK(uni_hal_io_buffer_pop_isr(&io_ctx.buf_rx, &byte, 1U, &woken) == 1U);
    } else {
        CHECK(uni_hal_io_receive_data(&io_ctx, &byte, 1U, 0) == 1U);
    }
    CHECK(byte == 'X');
    CHECK(!uni_hal_io_receive_sync(&io_ctx, marker, 3, 0));
    uni_hal_io_deinit(&io_ctx);
    return true;
}

static bool always_ready(uint32_t timeout, unsigned expected_reads) {
    UNI_HAL_IO_DEFINITION(io, 4, 3);
    const uint8_t marker[] = "Z";
    CHECK(uni_hal_io_init(&io_ctx));
    CHECK(send_bytes(&io_ctx, "A"));
    reads = 0;
    refill_bytes = 12;
    now_ms = UINT32_MAX - 1U;
    CHECK(!uni_hal_io_receive_sync(&io_ctx, marker, 1, timeout));
    CHECK(reads == expected_reads);
    CHECK(uni_hal_io_receive_available(&io_ctx) == 1U);
    refill_bytes = 0;
    uni_hal_io_deinit(&io_ctx);
    return true;
}

static bool final_byte(uint32_t delay) {
    UNI_HAL_IO_DEFINITION(io, 5, 3);
    const uint8_t marker[] = "AB";
    CHECK(uni_hal_io_init(&io_ctx));
    CHECK(send_bytes(&io_ctx, "A"));
    CHECK(!uni_hal_io_receive_sync(&io_ctx, marker, 2, 0));
    waiting_for_byte = true;
    wake_after_ms = delay;
    last_wait = 0;
    now_ms = UINT32_MAX - 1U;
    CHECK(uni_hal_io_receive_sync(&io_ctx, marker, 2, 3) == (delay == 3));
    CHECK(last_wait > 0U);
    CHECK(uni_hal_io_receive_available(&io_ctx) == 0U);
    if (delay > 3) {
        CHECK(uni_hal_io_receive_sync(&io_ctx, marker, 2, 0));
    }
    CHECK(!uni_hal_io_receive_sync(&io_ctx, marker, 2, 0));
    uni_hal_io_deinit(&io_ctx);
    return true;
}

int main(void) {
    bool ok = changed_pattern();
    ok = overlapping_fragments() && ok;
    ok = reusable_marker() && ok;
    ok = another_read(false) && ok;
    ok = another_read(true) && ok;
    ok = always_ready(3, 3) && ok;
    ok = always_ready(0, 1) && ok;
    ok = final_byte(3) && ok;
    ok = final_byte(7) && ok;
    return ok ? 0 : 1;
}
