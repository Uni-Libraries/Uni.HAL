//
// Includes
//

// uni.hal
#include "spi/uni_hal_spi.h"



//
// Implementation
//

bool uni_hal_spi_set_callback(uni_hal_spi_context_t *ctx, uni_hal_spi_callback_t callback, void *cookie) {
    bool result = false;
    if (uni_hal_spi_is_inited(ctx)) {
        ctx->status.callback = callback;
        ctx->status.callback_cookie = cookie;
        result = true;
    }
    return result;
}
