#include <stdio.h>
#include <string.h>

#include "io/uni_hal_io.h"

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

int main(void) {
    bool ok = changed_pattern();
    ok = overlapping_fragments() && ok;
    ok = reusable_marker() && ok;
    ok = another_read(false) && ok;
    ok = another_read(true) && ok;
    return ok ? 0 : 1;
}
