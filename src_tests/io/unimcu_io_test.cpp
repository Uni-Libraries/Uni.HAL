#include <cstring>

#include <catch2/catch_test_macros.hpp>

#include "io/uni_hal_io.h"

namespace {
struct Io {
    UNI_HAL_IO_DEFINITION(io, 16U, 6U);
    BaseType_t woken = pdFALSE;

    Io() { REQUIRE(uni_hal_io_init(&io_ctx)); }
    ~Io() { uni_hal_io_deinit(&io_ctx); }

    size_t push(const uint8_t *data, size_t size) {
        return uni_hal_io_buffer_push_isr(&io_ctx.buf_rx, data, size, &woken);
    }
};
}

TEST_CASE_METHOD(Io, "IO reinitialization clears both streams and a failed init can be retried", "[hal_io]") {
    REQUIRE_FALSE(uni_hal_io_init(nullptr));
    uint8_t byte = 0x55;
    REQUIRE(push(&byte, 1) == 1);
    REQUIRE(uni_hal_io_transmit_data(&io_ctx, &byte, 1) == 1);
    REQUIRE(uni_hal_io_init(&io_ctx));
    REQUIRE(uni_hal_io_receive_available(&io_ctx) == 0);
    REQUIRE(uni_hal_io_buffer_is_empty(&io_ctx.buf_tx));

    io_ctx.buf_tx.size = 0;
    REQUIRE_FALSE(uni_hal_io_init(&io_ctx));
    REQUIRE(io_ctx.buf_rx.handle == nullptr);
    REQUIRE(io_ctx.buf_tx.handle == nullptr);
    REQUIRE_FALSE(uni_hal_io_receive_clear(&io_ctx));
    io_ctx.buf_tx.size = 6;
    REQUIRE(uni_hal_io_init(&io_ctx));
    REQUIRE(uni_hal_io_transmit_data(&io_ctx, &byte, 1) == 1);
}

TEST_CASE_METHOD(Io, "IO reads FIFO bytes and clears pending input", "[hal_io]") {
    const uint8_t data[] = {0xA5, 0, 0xFF, 0x01};
    uint8_t output[sizeof(data)]{};
    REQUIRE(uni_hal_io_receive_available(nullptr) == 0);
    REQUIRE_FALSE(uni_hal_io_receive_clear(nullptr));
    REQUIRE(uni_hal_io_receive_data(&io_ctx, output, sizeof(output), 0) == 0);
    REQUIRE(push(data, sizeof(data)) == sizeof(data));
    REQUIRE(uni_hal_io_receive_available(&io_ctx) == sizeof(data));
    REQUIRE(uni_hal_io_receive_data(&io_ctx, output, 2, 0) == 2);
    REQUIRE(std::memcmp(output, data, 2) == 0);
    REQUIRE(uni_hal_io_receive_available(&io_ctx) == 2);
    REQUIRE(uni_hal_io_receive_data(&io_ctx, output, sizeof(output), 0) == 2);
    REQUIRE(std::memcmp(output, data + 2, 2) == 0);
    REQUIRE(push(data, sizeof(data)) == sizeof(data));
    REQUIRE(uni_hal_io_receive_clear(&io_ctx));
    REQUIRE(uni_hal_io_receive_available(&io_ctx) == 0);
    REQUIRE(uni_hal_io_receive_data(&io_ctx, output, sizeof(output), 0) == 0);
}

TEST_CASE_METHOD(Io, "IO synchronization consumes a fragmented delimiter and preserves the next frame", "[hal_io]") {
    const uint8_t delimiter[] = {'\r', '\n'};
    const uint8_t first[] = {'A', '\r'};
    const uint8_t rest[] = {'\n', 'B', '\r', '\n'};
    REQUIRE(push(first, sizeof(first)) == sizeof(first));
    REQUIRE_FALSE(uni_hal_io_receive_sync(nullptr, delimiter, sizeof(delimiter), 0));
    REQUIRE_FALSE(uni_hal_io_receive_sync(&io_ctx, nullptr, sizeof(delimiter), 0));
    REQUIRE_FALSE(uni_hal_io_receive_sync(&io_ctx, delimiter, 0, 0));
    REQUIRE(uni_hal_io_receive_available(&io_ctx) == sizeof(first));
    REQUIRE_FALSE(uni_hal_io_receive_sync(&io_ctx, delimiter, sizeof(delimiter), 0));
    REQUIRE(uni_hal_io_receive_available(&io_ctx) == 0);
    REQUIRE(push(rest, sizeof(rest)) == sizeof(rest));
    REQUIRE(uni_hal_io_receive_sync(&io_ctx, delimiter, sizeof(delimiter), 0));
    REQUIRE(uni_hal_io_receive_available(&io_ctx) == sizeof(rest) - 1);
    uint8_t output[sizeof(rest)]{};
    REQUIRE(uni_hal_io_receive_data(&io_ctx, output, sizeof(output), 0) == sizeof(rest) - 1);
    REQUIRE(std::memcmp(output, rest + 1, sizeof(rest) - 1) == 0);
}

TEST_CASE_METHOD(Io, "IO transmit reports accepted bytes and triggers only after queuing them", "[hal_io]") {
    struct {
        void *sender = nullptr;
        size_t queued = 0;
        unsigned calls = 0;
    } trigger;
    io_ctx.handlers.tx_trigger_ctx = &trigger;
    io_ctx.handlers.tx_trigger = [](void *io, void *user) {
        auto *record = static_cast<decltype(trigger) *>(user);
        record->sender = io;
        record->queued = xStreamBufferBytesAvailable(static_cast<uni_hal_io_context_t *>(io)->buf_tx.handle);
        ++record->calls;
    };
    const uint8_t data[] = {5, 4, 3, 2, 1, 0};
    REQUIRE(uni_hal_io_transmit_data(nullptr, data, sizeof(data)) == 0);
    REQUIRE(uni_hal_io_transmit_data(&io_ctx, nullptr, sizeof(data)) == 0);
    REQUIRE(trigger.calls == 0);
    REQUIRE(uni_hal_io_transmit_data(&io_ctx, data, sizeof(data)) == 5);
    REQUIRE(trigger.sender == &io_ctx);
    REQUIRE(trigger.queued == 5);
    REQUIRE(trigger.calls == 1);
    REQUIRE(uni_hal_io_transmit_data(&io_ctx, data + 5, 1) == 0);
    REQUIRE(trigger.calls == 1);
    uint8_t output[sizeof(data)]{};
    REQUIRE(uni_hal_io_buffer_pop_isr(&io_ctx.buf_tx, output, sizeof(output), &woken) == 5);
    REQUIRE(std::memcmp(output, data, 5) == 0);
    REQUIRE(uni_hal_io_transmit_data(&io_ctx, data + 5, 1) == 1);
    REQUIRE(trigger.queued == 1);
    REQUIRE(trigger.calls == 2);
    REQUIRE(uni_hal_io_buffer_pop_isr(&io_ctx.buf_tx, output, 1, &woken) == 1);
    REQUIRE(output[0] == data[5]);
}
