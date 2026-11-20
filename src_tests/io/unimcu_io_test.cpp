#include <cstring>

#include <catch2/catch_test_macros.hpp>

#include "io/uni_hal_io.h"
#include "io/uni_hal_io_tunnel.h"

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

TEST_CASE("IO tunnel disconnect releases both endpoints and permits reconnection", "[hal_io]") {
    Io first, second, third;
    uni_hal_io_tunnel_context_t tunnel{};
    REQUIRE(uni_hal_io_tunnel_init(&tunnel, &first.io_ctx, &second.io_ctx, false));
    REQUIRE(uni_hal_io_tunnel_init(&tunnel, nullptr, nullptr, false));
    REQUIRE_FALSE(uni_hal_io_tunnel_is_inited(&tunnel));
    REQUIRE(first.io_ctx.handlers.tunnel_ctx == nullptr);
    REQUIRE(second.io_ctx.handlers.tunnel_ctx == nullptr);
    uint8_t byte = 0x5A;
    REQUIRE(uni_hal_io_tunnel_transmit(&tunnel, &first.io_ctx, &byte, 1) == UNI_HAL_IO_TUNNEL_FAIL);
    REQUIRE(uni_hal_io_tunnel_init(&tunnel, nullptr, nullptr, false));

    REQUIRE(uni_hal_io_tunnel_init(&tunnel, &first.io_ctx, &third.io_ctx, true));
    REQUIRE_FALSE(uni_hal_io_tunnel_init(&tunnel, &second.io_ctx, nullptr, false));
    REQUIRE_FALSE(uni_hal_io_tunnel_init(&tunnel, nullptr, &second.io_ctx, false));
    Io fourth;
    uni_hal_io_tunnel_context_t other{};
    REQUIRE(uni_hal_io_tunnel_init(&other, &second.io_ctx, &fourth.io_ctx, false));
    REQUIRE_FALSE(uni_hal_io_tunnel_init(&other, &first.io_ctx, &fourth.io_ctx, false));
    REQUIRE_FALSE(uni_hal_io_tunnel_init(&other, &second.io_ctx, &third.io_ctx, false));
    REQUIRE(other.io_first == &second.io_ctx);
    REQUIRE(other.io_second == &fourth.io_ctx);
    REQUIRE(uni_hal_io_tunnel_init(&other, nullptr, nullptr, false));
    REQUIRE(uni_hal_io_tunnel_is_inited(&tunnel));
    REQUIRE(first.io_ctx.handlers.tunnel_ctx == &tunnel);
    REQUIRE(third.io_ctx.handlers.tunnel_ctx == &tunnel);
    REQUIRE(second.io_ctx.handlers.tunnel_ctx == nullptr);
    REQUIRE(uni_hal_io_tunnel_transmit(&tunnel, &first.io_ctx, &byte, 1) == UNI_HAL_IO_TUNNEL_TRANSPARENT);
    REQUIRE(uni_hal_io_buffer_pop_isr(&third.io_ctx.buf_tx, &byte, 1, &third.woken) == 1);
    REQUIRE(byte == 0x5A);
}

TEST_CASE("IO tunnel forwards a whole block once or leaves room for a retry", "[hal_io]") {
    for (bool transparent : {false, true}) {
        Io source, peer;
        uni_hal_io_tunnel_context_t tunnel{};
        REQUIRE(uni_hal_io_tunnel_init(&tunnel, &source.io_ctx, &peer.io_ctx, transparent));
        uint8_t queued[] = {0xBA, 0xAD};
        uint8_t block[] = {1, 2, 3, 4, 5};
        uint8_t output[sizeof(block)]{};
        REQUIRE(uni_hal_io_transmit_data(&peer.io_ctx, queued, sizeof(queued)) == sizeof(queued));
        unsigned triggers = 0;
        peer.io_ctx.handlers.tx_trigger_ctx = &triggers;
        peer.io_ctx.handlers.tx_trigger = [](void *, void *ctx) { ++*static_cast<unsigned *>(ctx); };

        REQUIRE(uni_hal_io_tunnel_transmit(&tunnel, &source.io_ctx, block, sizeof(block)) == UNI_HAL_IO_TUNNEL_FAIL);
        REQUIRE(uni_hal_io_tunnel_transmit(&tunnel, &source.io_ctx, block, 0) == UNI_HAL_IO_TUNNEL_FAIL);
        REQUIRE(triggers == 0);
        REQUIRE(uni_hal_io_buffer_pop_isr(&peer.io_ctx.buf_tx, output, sizeof(output), &peer.woken) == sizeof(queued));
        REQUIRE(std::memcmp(output, queued, sizeof(queued)) == 0);

        const auto expected = transparent ? UNI_HAL_IO_TUNNEL_TRANSPARENT : UNI_HAL_IO_TUNNEL_OK;
        REQUIRE(uni_hal_io_tunnel_transmit(&tunnel, &source.io_ctx, block, sizeof(block)) == expected);
        REQUIRE(triggers == 1);
        REQUIRE(uni_hal_io_buffer_pop_isr(&peer.io_ctx.buf_tx, output, sizeof(output), &peer.woken) == sizeof(block));
        REQUIRE(std::memcmp(output, block, sizeof(block)) == 0);
    }
}
