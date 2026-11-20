#include "io/uni_hal_io.h"

namespace {
struct Device {
    UNI_HAL_IO_DEFINITION(port, 5U, 9U);
};

// Firmware can declare the context and its storage before C++ startup runs.
constinit Device global_device;

bool check_device(Device &device) {
    auto &ctx = device.port_ctx;
    if (ctx.buf_rx.size != 5U || ctx.buf_tx.size != 9U ||
        ctx.buf_rx.handle || ctx.buf_tx.handle || ctx.handlers.tx_trigger ||
        ctx.stats.rx_received || ctx.sync_pattern_len) {
        return false;
    }
#if configSUPPORT_STATIC_ALLOCATION
    if (ctx.buf_rx.array != device.port_rx_buf || ctx.buf_tx.array != device.port_tx_buf) {
        return false;
    }
#else
    if (ctx.buf_rx.array || ctx.buf_tx.array) {
        return false;
    }
#endif
    if (!uni_hal_io_init(&ctx)) {
        return false;
    }
    bool const capacities_match = xStreamBufferSpacesAvailable(ctx.buf_rx.handle) == 4U &&
                                  xStreamBufferSpacesAvailable(ctx.buf_tx.handle) == 8U;
    uni_hal_io_deinit(&ctx);
    return capacities_match;
}
}

extern "C" int uni_hal_test_cpp_io_definition(void) {
    Device local_device;
    return check_device(global_device) && check_device(local_device) ? 0 : 30;
}
