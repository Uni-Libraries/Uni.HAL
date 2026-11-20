#include "i2c/uni_hal_i2c.h"
#include "systick/uni_hal_systick.h"

bool uni_hal_i2c_init(uni_hal_i2c_context_t *ctx) {
    if (!ctx) {
        return false;
    }
    ctx->state.initialized = true;
    return true;
}

bool uni_hal_i2c_deinit(uni_hal_i2c_context_t *ctx) {
    if (!ctx) {
        return false;
    }
    ctx->state.initialized = false;
    return true;
}

static bool exchange(uni_hal_i2c_context_t *ctx, uni_hal_i2c_pc_request_t request) {
    if (!uni_hal_i2c_is_inited(ctx) || !ctx->config.pc_exchange || request.timeout == 0U ||
        request.dev_addr > UINT8_MAX) {
        return false;
    }
    if (request.operation == UNI_HAL_I2C_PC_PROBE) {
        if (request.trials == 0U) {
            return false;
        }
    } else if (!request.buf || !request.buf_len) {
        return false;
    }
    if (request.mem_addr_size == UNI_HAL_I2C_MEMADD_SIZE_8BIT) {
        request.mem_addr &= UINT8_MAX;
    } else if (request.mem_addr_size != UNI_HAL_I2C_MEMADD_SIZE_16BIT) {
        return false;
    }
    if (request.dev_addr > 0x7FU) {
        request.dev_addr >>= 1U;
    }
    uint32_t const start = uni_hal_systick_get_ms();
    int32_t const bytes = ctx->config.pc_exchange(ctx->config.pc_user, &request);
    return bytes == request.buf_len && uni_hal_systick_get_ms() - start <= request.timeout;
}

bool uni_hal_i2c_isready(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint32_t trials, uint32_t timeout) {
    return exchange(ctx, (uni_hal_i2c_pc_request_t){
        .operation = UNI_HAL_I2C_PC_PROBE, .dev_addr = dev_addr, .trials = trials, .timeout = timeout,
    });
}

bool uni_hal_i2c_master_receive(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint8_t *buf, uint16_t len, uint32_t timeout) {
    return exchange(ctx, (uni_hal_i2c_pc_request_t){
        .operation = UNI_HAL_I2C_PC_MASTER_RECEIVE, .dev_addr = dev_addr, .buf = buf, .buf_len = len, .timeout = timeout,
    });
}

bool uni_hal_i2c_master_transmit(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint8_t *buf, uint16_t len, uint32_t timeout) {
    return exchange(ctx, (uni_hal_i2c_pc_request_t){
        .operation = UNI_HAL_I2C_PC_MASTER_TRANSMIT, .dev_addr = dev_addr, .buf = buf, .buf_len = len, .timeout = timeout,
    });
}

bool uni_hal_i2c_mem_read(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint16_t mem_addr, uni_hal_i2c_memadd_size_e size,
                          uint8_t *buf, uint16_t len, uint32_t timeout) {
    return exchange(ctx, (uni_hal_i2c_pc_request_t){
        .operation = UNI_HAL_I2C_PC_MEM_READ, .dev_addr = dev_addr, .mem_addr = mem_addr, .mem_addr_size = size,
        .buf = buf, .buf_len = len, .timeout = timeout,
    });
}

bool uni_hal_i2c_mem_write(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint16_t mem_addr, uni_hal_i2c_memadd_size_e size,
                           uint8_t *buf, uint16_t len, uint32_t timeout) {
    return exchange(ctx, (uni_hal_i2c_pc_request_t){
        .operation = UNI_HAL_I2C_PC_MEM_WRITE, .dev_addr = dev_addr, .mem_addr = mem_addr, .mem_addr_size = size,
        .buf = buf, .buf_len = len, .timeout = timeout,
    });
}
