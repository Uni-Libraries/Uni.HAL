//
// Includes
//

// stdlib
#include <math.h>

// Uni.HAL
#include "uni_common_bytes.h"
#include "periph/uni_hal_periph_ads1015.h"



//
// Private
//

static bool _uni_hal_ads1015_read(uni_hal_ads1015_context_t* ctx, uint8_t address, uint16_t* val)
{
    bool result = false;
    if (uni_hal_ads1015_is_inited(ctx) && val)
    {
        result = uni_hal_i2c_mem_read(ctx->config.i2c, ctx->config.address, address, UNI_HAL_I2C_MEMADD_SIZE_8BIT, (uint8_t*)val,
                                     sizeof(*val), ctx->config.timeout);
        *val = uni_common_bytes_swap16(*val);
    }
    return result;
}


static bool _uni_hal_ads1015_write(uni_hal_ads1015_context_t* ctx, uint8_t reg_addr, uint16_t reg_value) {
    bool result = false;
    if (uni_hal_ads1015_is_inited(ctx)) {
        reg_value = uni_common_bytes_swap16(reg_value);
        result = uni_hal_i2c_mem_write(ctx->config.i2c, ctx->config.address, reg_addr,UNI_HAL_I2C_MEMADD_SIZE_8BIT, (uint8_t*)&reg_value, sizeof(reg_value), ctx->config.timeout);
    }
    return result;
}



//
// Functions
//

bool uni_hal_ads1015_init(uni_hal_ads1015_context_t* ctx) {
    bool result = false;
    if (ctx != NULL && ctx->state.initialized != true) {
        result = uni_hal_i2c_is_inited(ctx->config.i2c);
        if (!result) {
            result = uni_hal_i2c_init(ctx->config.i2c);
        }
        ctx->state.initialized = result;
        result = uni_hal_ads1015_configure(ctx);
    }

    return result;
}


bool uni_hal_ads1015_is_inited(const uni_hal_ads1015_context_t* ctx) {
    return ctx != NULL && ctx->state.initialized != false;
}

bool uni_hal_ads1015_configure(uni_hal_ads1015_context_t* ctx)
{
    bool result = false;
    if (uni_hal_ads1015_is_inited(ctx))
    {
        uint16_t config = UNI_HAL_ADS1015_STATUS_NOEFF | ctx->config.mux | ctx->config.pga | ctx->config.mode | ctx->config.rate;
        result = _uni_hal_ads1015_write(ctx, UNI_HAL_ADS1015_REG_CONFIG, config);
    }

    return result;
}


uint16_t uni_hal_ads1015_get_raw(uni_hal_ads1015_context_t* ctx)
{
    uint16_t result = UINT16_MAX;
    if (uni_hal_ads1015_is_inited(ctx))
    {
        if (_uni_hal_ads1015_read(ctx, UNI_HAL_ADS1015_REG_CONVERSION, &result))
        {
            result = result >> 4U;
        }
        else{
            result = UINT16_MAX;
        }
    }

    return result;
}


int16_t uni_hal_ads1015_get_voltage_mv(uni_hal_ads1015_context_t* ctx)
{
    uint16_t result = uni_hal_ads1015_get_raw(ctx);
    bool is_neg = false;
    if (result != UINT16_MAX)
    {
        if(result > 0x7FF)
        {
            is_neg = true;
        }

        switch(ctx->config.pga) {
                case UNI_HAL_ADS1015_CONFIG_PGA_6144:
                    result *= 3;
                    break;
                case UNI_HAL_ADS1015_CONFIG_PGA_4096:
                    result *= 2;
                    break;
                case UNI_HAL_ADS1015_CONFIG_PGA_2048:
                    result *= 1;
                    break;
                case UNI_HAL_ADS1015_CONFIG_PGA_1024:
                    result /= 2;
                    break;
                case UNI_HAL_ADS1015_CONFIG_PGA_0512:
                    result /= 4;
                    break;
                case UNI_HAL_ADS1015_CONFIG_PGA_0256:
                    result /= 8;
                    break;
                default:
                    break;
        }

        if (is_neg)
        {
            result |= 0x8000;
        }
    }
    return result;
}