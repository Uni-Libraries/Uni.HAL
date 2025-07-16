//
// Includes
//

// stdlib
#include <math.h>

// FreeRTOS
#include <FreeRTOS.h>

// Uni.Common
#include <uni_common.h>

// Uni.HAL
#include "uni_hal_periph_ads1015.h"



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


int16_t uni_hal_ads1015_get_raw(uni_hal_ads1015_context_t* ctx)
{
    int16_t result = INT16_MAX;
    if (!uni_hal_ads1015_is_inited(ctx)) {
        return INT16_MAX;
    }

    if (ctx->config.mode == UNI_HAL_ADS1015_MODE_SINGLE) {
        uint16_t config_val = UNI_HAL_ADS1015_STATUS_NOEFF | ctx->config.mux | ctx->config.pga | ctx->config.mode | ctx->config.rate;
        config_val |= (1 << 15); // Trigger conversion

        if (!_uni_hal_ads1015_write(ctx, UNI_HAL_ADS1015_REG_CONFIG, config_val)) {
            return INT16_MAX;
        }

        // Wait for the conversion to complete
        while (!uni_hal_ads1015_is_ready(ctx)) {
            portYIELD();
        }
    }

    if (_uni_hal_ads1015_read(ctx, UNI_HAL_ADS1015_REG_CONVERSION, (uint16_t*) &result))
    {
        result = uni_common_bytes_i12_to_i16(result);
    } else {
        result = INT16_MAX;
    }

    return result;
}


int16_t uni_hal_ads1015_get_voltage_mv(uni_hal_ads1015_context_t* ctx)
{
    int16_t raw_value = uni_hal_ads1015_get_raw(ctx);

    if (raw_value == INT16_MAX) {
        return INT16_MAX;
    }

    float mv_per_lsb;

    switch (ctx->config.pga) {
        case UNI_HAL_ADS1015_CONFIG_PGA_6144:
            mv_per_lsb = 3.0F;
            break;
        case UNI_HAL_ADS1015_CONFIG_PGA_4096:
            mv_per_lsb = 2.0F;
            break;
        case UNI_HAL_ADS1015_CONFIG_PGA_2048:
            mv_per_lsb = 1.0F;
            break;
        case UNI_HAL_ADS1015_CONFIG_PGA_1024:
            mv_per_lsb = 0.5F;
            break;
        case UNI_HAL_ADS1015_CONFIG_PGA_0512:
            mv_per_lsb = 0.25F;
            break;
        case UNI_HAL_ADS1015_CONFIG_PGA_0256:
            mv_per_lsb = 0.125F;
            break;
        default:
            mv_per_lsb = 0.0F;
            break;
    }

    return (int16_t)((float)raw_value * mv_per_lsb);
}

bool uni_hal_ads1015_is_ready(uni_hal_ads1015_context_t* ctx)
{
    bool result = false;
    if (uni_hal_ads1015_is_inited(ctx))
    {
        uint16_t config_reg;
        if(_uni_hal_ads1015_read(ctx, UNI_HAL_ADS1015_REG_CONFIG, &config_reg))
        {
            // Bit 15 is the OS bit. 1 means conversion is done.
            result = (config_reg & 0x8000) != 0;
        }
    }
    return result;
}
