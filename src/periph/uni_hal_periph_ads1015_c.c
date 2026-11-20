//
// Includes
//

// Uni.HAL OS layer
#include "os/uni_hal_os.h"

// Uni.Common
#include <uni_common.h>

// Uni.HAL
#include "uni_hal_periph_ads1015.h"
#include "systick/uni_hal_systick.h"



//
// Private
//

// The OS bit starts a single conversion on write and reports idle on read.
#define UNI_HAL_ADS1015_CONFIG_OS (1U << 15)

static uint16_t _uni_hal_ads1015_config_word(const uni_hal_ads1015_config_t *config) {
    return (uint16_t)config->mux | (uint16_t)config->pga | (uint16_t)config->mode | (uint16_t)config->rate;
}

static bool _uni_hal_ads1015_read(uni_hal_ads1015_context_t *ctx, uint8_t reg, uint16_t *value, uint32_t timeout) {
    uint8_t bytes[2];
    if (!uni_hal_i2c_mem_read(ctx->config.i2c, ctx->config.address, reg, UNI_HAL_I2C_MEMADD_SIZE_8BIT,
                              bytes, sizeof(bytes), timeout)) {
        return false;
    }
    *value = ((uint16_t)bytes[0] << 8) | bytes[1];
    return true;
}

static bool _uni_hal_ads1015_write(uni_hal_ads1015_context_t *ctx, uint8_t reg, uint16_t value, uint32_t timeout) {
    uint8_t bytes[] = {(uint8_t)(value >> 8), (uint8_t)value};
    return uni_hal_i2c_mem_write(ctx->config.i2c, ctx->config.address, reg, UNI_HAL_I2C_MEMADD_SIZE_8BIT,
                                 bytes, sizeof(bytes), timeout);
}

static uint32_t _uni_hal_ads1015_remaining_ms(uint32_t start, uint32_t timeout) {
    uint32_t const elapsed = uni_hal_systick_get_ms() - start;
    return elapsed < timeout ? timeout - elapsed : 0U;
}

static bool _uni_hal_ads1015_single_command(uni_hal_ads1015_context_t *ctx, uint32_t start, uint16_t os) {
    uint32_t const remaining = _uni_hal_ads1015_remaining_ms(start, ctx->config.timeout);
    uint16_t const config = _uni_hal_ads1015_config_word(&ctx->config) | os;
    return remaining && _uni_hal_ads1015_write(ctx, UNI_HAL_ADS1015_REG_CONFIG, config, remaining);
}

static bool _uni_hal_ads1015_wait_idle(uni_hal_ads1015_context_t *ctx, uint32_t start, bool stop_continuous) {
    for (;;) {
        uint16_t status;
        uint32_t const remaining = _uni_hal_ads1015_remaining_ms(start, ctx->config.timeout);
        if (!remaining || !_uni_hal_ads1015_read(ctx, UNI_HAL_ADS1015_REG_CONFIG, &status, remaining)) {
            return false;
        }
        if (status & UNI_HAL_ADS1015_CONFIG_OS) {
            return true;
        }
        if (stop_continuous && !(status & UNI_HAL_ADS1015_MODE_SINGLE)) {
            // Finish the active continuous conversion and power down before starting a single shot.
            if (!_uni_hal_ads1015_single_command(ctx, start, 0U)) {
                return false;
            }
            stop_continuous = false;
        }
        if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
            UNI_HAL_OS_YIELD();
        }
    }
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
        // configure() needs the flag set, but the device only counts as initialised once it took the config
        ctx->state.initialized = result;
        result = uni_hal_ads1015_configure(ctx);
        ctx->state.initialized = result;
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
        uint16_t const config = _uni_hal_ads1015_config_word(&ctx->config);
        result = _uni_hal_ads1015_write(ctx, UNI_HAL_ADS1015_REG_CONFIG, config, ctx->config.timeout);
    }

    return result;
}


int16_t uni_hal_ads1015_get_raw(uni_hal_ads1015_context_t* ctx)
{
    if (!uni_hal_ads1015_is_inited(ctx) || ctx->config.timeout == 0U) {
        return INT16_MAX;
    }

    uint32_t const timeout = ctx->config.timeout;
    uint32_t const start = uni_hal_systick_get_ms();
    if (ctx->config.mode == UNI_HAL_ADS1015_MODE_SINGLE) {
        // OS=1 is ignored while a previous conversion is running, even if the I2C write succeeds.
        if (!_uni_hal_ads1015_wait_idle(ctx, start, true) ||
            !_uni_hal_ads1015_single_command(ctx, start, UNI_HAL_ADS1015_CONFIG_OS) ||
            !_uni_hal_ads1015_wait_idle(ctx, start, false)) {
            return INT16_MAX;
        }
    }

    uint16_t sample;
    uint32_t const remaining = _uni_hal_ads1015_remaining_ms(start, timeout);
    if (remaining == 0U || !_uni_hal_ads1015_read(ctx, UNI_HAL_ADS1015_REG_CONVERSION, &sample, remaining) ||
        uni_hal_systick_get_ms() - start > timeout) {
        return INT16_MAX;
    }
    return uni_common_bytes_i12_to_i16(sample >> 4);
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

uni_hal_ads1015_answer_e uni_hal_ads1015_is_ready(uni_hal_ads1015_context_t* ctx)
{
    if (!uni_hal_ads1015_is_inited(ctx)) {
        return UNI_HAL_ADS1015_ANSWER_FAILED;
    }
    uint16_t config;
    if (!_uni_hal_ads1015_read(ctx, UNI_HAL_ADS1015_REG_CONFIG, &config, ctx->config.timeout)) {
        return UNI_HAL_ADS1015_ANSWER_FAILED;
    }
    return (config & UNI_HAL_ADS1015_CONFIG_OS) ? UNI_HAL_ADS1015_ANSWER_OK : UNI_HAL_ADS1015_ANSWER_NOTREADY;
}
