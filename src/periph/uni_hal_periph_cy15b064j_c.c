//
// Includes
//

// Uni.HAL
#include "periph/uni_hal_periph_cy15b064j.h"


//
// Functions
//

bool uni_hal_c15b064j_init(uni_hal_c15b064j_context_t* ctx) {
    bool result = false;
    if (ctx != nullptr && ctx->state.initialized != true) {
        result = uni_hal_i2c_is_inited(ctx->config.i2c);
        if (!result) {
            result = uni_hal_i2c_init(ctx->config.i2c);
        }
        ctx->state.initialized = result;
    }

    return result;
}

bool uni_hal_c15b064j_is_inited(const uni_hal_c15b064j_context_t* ctx) {
    return ctx != nullptr && ctx->state.initialized != false;
}

bool uni_hal_c15b064j_read(uni_hal_c15b064j_context_t* ctx, uint16_t address, uint16_t len, uint8_t* buf) {
    bool result = false;
    if (uni_hal_c15b064j_is_inited(ctx)) {
        result = uni_hal_i2c_mem_read(ctx->config.i2c, ctx->config.address, address, UNI_HAL_I2C_MEMADD_SIZE_16BIT, buf,
                                     len, ctx->config.timeout);
    }
    return result;
}

bool uni_hal_c15b064j_write(uni_hal_c15b064j_context_t* ctx, uint16_t address, uint16_t len, uint8_t* buf) {
    bool result = false;
    if (uni_hal_c15b064j_is_inited(ctx)) {
        result = uni_hal_i2c_mem_write(ctx->config.i2c, ctx->config.address, address, UNI_HAL_I2C_MEMADD_SIZE_16BIT, buf,
                                      len, ctx->config.timeout);
    }
    return result;
}
