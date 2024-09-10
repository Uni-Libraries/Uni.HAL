//
// Includes
//

// stdlib
#include <stddef.h>

// uni_hal
#include "i2c/uni_hal_i2c.h"



//
// Functions
//

bool uni_hal_i2c_is_inited(uni_hal_i2c_context_t *ctx) {
    bool result = false;
    if (ctx != NULL) {
        result = ctx->state.initialized;
    }
    return result;
}


bool uni_hal_i2c_reset(uni_hal_i2c_context_t *ctx) {
    bool result = false;
    if (ctx != NULL) {
        uni_hal_i2c_deinit(ctx);
        result = uni_hal_i2c_init(ctx);
    }

    return result;
}
