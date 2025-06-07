//
// Includes
//

// stdlib
#include <stddef.h>

// Uni.HAL
#include "periph/uni_hal_periph_mcp23017.h"



//
// Private
//

static bool _uni_hal_mcp23017_read(uni_hal_mcp23017_context_t* ctx, uint8_t address, uint8_t* buf, uint16_t buf_len) {
    bool result = false;
    if (uni_hal_mcp23017_is_inited(ctx)) {
        result = uni_hal_i2c_mem_read(ctx->config.i2c, ctx->config.address, address, UNI_HAL_I2C_MEMADD_SIZE_8BIT, buf,
                                     buf_len, ctx->config.timeout);
    }
    return result;
}


static bool _uni_hal_mcp23017_write(uni_hal_mcp23017_context_t* ctx, uint8_t address, uint8_t* buf, uint16_t buf_len) {
    bool result = false;
    if (uni_hal_mcp23017_is_inited(ctx)) {
        result = uni_hal_i2c_mem_write(ctx->config.i2c, ctx->config.address, address, UNI_HAL_I2C_MEMADD_SIZE_8BIT, buf,
                                      buf_len, ctx->config.timeout);
    }
    return result;
}



//
// Functions
//

bool uni_hal_mcp23017_init(uni_hal_mcp23017_context_t* ctx) {
    bool result = false;
    if (ctx != NULL && ctx->state.initialized != true) {
        result = uni_hal_i2c_is_inited(ctx->config.i2c);
        if (!result) {
            result = uni_hal_i2c_init(ctx->config.i2c);
        }
        ctx->state.initialized = result;
    }

    return result;
}


bool uni_hal_mcp23017_is_inited(const uni_hal_mcp23017_context_t* ctx) {
    return ctx != NULL && ctx->state.initialized != false;
}


bool uni_hal_mcp23017_get_gpio(uni_hal_mcp23017_context_t* ctx, uni_hal_mcp23017_port_e port, uint8_t* val)
{
    uint8_t reg = 0U;
    switch (port)
    {
    case UNI_HAL_MCP23017_PORT_A:
        reg = UNI_HAL_MCP23017_REG_GPIO_A;
        break;
    case UNI_HAL_MCP23017_PORT_B:
        reg = UNI_HAL_MCP23017_REG_GPIO_B;
        break;
    default:
        break;
    }

    return _uni_hal_mcp23017_read(ctx, reg, val, sizeof(*val));
}


bool uni_hal_mcp23017_set_iodir(uni_hal_mcp23017_context_t* ctx, uni_hal_mcp23017_port_e port, uint8_t val)
{
    uint8_t reg = 0U;
    switch (port)
    {
        case UNI_HAL_MCP23017_PORT_A:
            reg = UNI_HAL_MCP23017_REG_IODIR_A;
            break;
        case UNI_HAL_MCP23017_PORT_B:
            reg = UNI_HAL_MCP23017_REG_IODIR_B;
            break;
        default:
            break;
    }

    return _uni_hal_mcp23017_write(ctx, reg, &val, sizeof(val));
}


bool uni_hal_mcp23017_set_gpio(uni_hal_mcp23017_context_t* ctx, uni_hal_mcp23017_port_e port, uint8_t val)
{
    uint8_t reg = 0U;
    switch (port)
    {
    case UNI_HAL_MCP23017_PORT_A:
        reg = UNI_HAL_MCP23017_REG_GPIO_A;
        break;
    case UNI_HAL_MCP23017_PORT_B:
        reg = UNI_HAL_MCP23017_REG_GPIO_B;
        break;
    default:
        break;
    }

    return _uni_hal_mcp23017_write(ctx, reg, &val, sizeof(val));
}
