//
// Includes
//

// stdlib
#include <stddef.h>

// st
#include <stm32h7xx_hal.h>
#include <stm32h7xx_ll_i2c.h>

// Uni.Common
#include <uni_common.h>

// Uni.HAL
#include "core/uni_hal_core_enum.h"
#include "i2c/uni_hal_i2c.h"
#include "systick/uni_hal_systick.h"



//
// Globals
//

UNI_COMMON_COMPILER_WEAK I2C_HandleTypeDef hi2c1 = {0};
UNI_COMMON_COMPILER_WEAK I2C_HandleTypeDef hi2c2 = {0};
UNI_COMMON_COMPILER_WEAK I2C_HandleTypeDef hi2c3 = {0};
UNI_COMMON_COMPILER_WEAK I2C_HandleTypeDef hi2c4 = {0};


//
// Private
//

/**
 * Convert memory address size to the HAL enum
 * @param size memory address size enum value
 * @return HAL value
 */
static uint32_t _uni_hal_i2c_memadd_size(uni_hal_i2c_memadd_size_e size) {
    uint32_t result = 0;
    switch (size) {
    case UNI_HAL_I2C_MEMADD_SIZE_8BIT:
        result = I2C_MEMADD_SIZE_8BIT;
        break;
    case UNI_HAL_I2C_MEMADD_SIZE_16BIT:
        result = I2C_MEMADD_SIZE_16BIT;
        break;
    default:
        break;
    }
    return result;
}

static uint16_t _uni_hal_i2c_shiftaddr(uint16_t addr) {
    uint16_t result = addr;
    if (result < UINT8_MAX && !(result & (1 << 7))) {
        result = result << 1;
    }

    return result;
}

typedef struct {
    I2C_TypeDef *instance;
    I2C_HandleTypeDef *hal;
    uni_hal_i2c_context_t *owner;
} i2c_bus_t;

static i2c_bus_t buses[] = {
    {I2C1, &hi2c1, nullptr},
    {I2C2, &hi2c2, nullptr},
    {I2C3, &hi2c3, nullptr},
    {I2C4, &hi2c4, nullptr},
};

static i2c_bus_t *get_bus(uni_hal_core_periph_e instance) {
    if (instance < UNI_HAL_CORE_PERIPH_I2C_1 || instance > UNI_HAL_CORE_PERIPH_I2C_4) {
        return nullptr;
    }
    return &buses[instance - UNI_HAL_CORE_PERIPH_I2C_1];
}

static I2C_HandleTypeDef *_uni_hal_i2c_get_handle_hal(uni_hal_core_periph_e instance) {
    i2c_bus_t *bus = get_bus(instance);
    return bus ? bus->hal : nullptr;
}



//
// Public
//

bool uni_hal_i2c_init(uni_hal_i2c_context_t *ctx) {
    i2c_bus_t *bus = ctx ? get_bus(ctx->config.instance) : nullptr;
    if (!bus || bus->owner || ctx->state.initialized) {
        return false;
    }

    uni_hal_rcc_clksrc_set(ctx->config.instance, ctx->config.clock_source);
    if (!uni_hal_rcc_clk_set(ctx->config.instance, true)) {
        return false;
    }
    if (!uni_hal_gpio_pin_is_inited(ctx->config.pin_sck) && !uni_hal_gpio_pin_init(ctx->config.pin_sck)) {
        return false;
    }
    if (!uni_hal_gpio_pin_is_inited(ctx->config.pin_sda) && !uni_hal_gpio_pin_init(ctx->config.pin_sda)) {
        return false;
    }

    I2C_HandleTypeDef *hal = bus->hal;
    hal->Init.Timing = ctx->config.timing != 0U ? ctx->config.timing
            : uni_hal_i2c_timing_calc(uni_hal_rcc_clk_get_freq(ctx->config.instance), ctx->config.speed);
    if (hal->Init.Timing == 0U) {
        return false;
    }
    hal->Init.OwnAddress1 = 0;
    hal->Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hal->Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hal->Init.OwnAddress2 = 0;
    hal->Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hal->Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hal->Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    bus->owner = ctx;
    hal->Instance = bus->instance;
    if (HAL_I2C_Init(hal) != HAL_OK ||
        HAL_I2CEx_ConfigAnalogFilter(hal, I2C_ANALOGFILTER_ENABLE) != HAL_OK ||
        HAL_I2CEx_ConfigDigitalFilter(hal, 0) != HAL_OK) {
        (void)uni_hal_i2c_deinit(ctx);
        return false;
    }

    ctx->state.initialized = true;
    return true;
}


bool uni_hal_i2c_deinit(uni_hal_i2c_context_t *ctx) {
    i2c_bus_t *bus = ctx ? get_bus(ctx->config.instance) : nullptr;
    if (!bus || (bus->owner && bus->owner != ctx)) {
        return false;
    }
    ctx->state.initialized = false;
    if (bus->hal->Instance) {
        if (bus->hal->Instance != bus->instance || HAL_I2C_DeInit(bus->hal) != HAL_OK) {
            return false;
        }
        bus->hal->Instance = nullptr;
    }
    // A bootloader can leave the peripheral enabled even when our HAL handle is still empty.
    if (LL_I2C_DeInit(bus->instance) != SUCCESS) {
        return false;
    }
    bus->owner = nullptr;
    return true;
}


bool uni_hal_i2c_isready(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint32_t trials, uint32_t timeout) {
    bool result = false;

    if (uni_hal_i2c_is_inited(ctx)) {
        HAL_StatusTypeDef status = HAL_BUSY;
        uint32_t const start_ms = uni_hal_systick_get_ms();
        while (status == HAL_BUSY && (uni_hal_systick_get_ms() - start_ms) <= timeout) {
            status = HAL_I2C_IsDeviceReady(_uni_hal_i2c_get_handle_hal(ctx->config.instance), _uni_hal_i2c_shiftaddr(dev_addr), trials, timeout);
        }

        result = status == HAL_OK;
    }

    return result;
}


bool uni_hal_i2c_master_receive(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint8_t *buf, uint16_t buf_len, uint32_t timeout) {
    bool result = false;

    if (uni_hal_i2c_is_inited(ctx)) {
        HAL_StatusTypeDef status = HAL_BUSY;
        uint32_t const start_ms = uni_hal_systick_get_ms();
        while (status == HAL_BUSY && (uni_hal_systick_get_ms() - start_ms) <= timeout) {
            status =
                HAL_I2C_Master_Receive(_uni_hal_i2c_get_handle_hal(ctx->config.instance), _uni_hal_i2c_shiftaddr(dev_addr), buf, buf_len, timeout);
        }

        result = status == HAL_OK;
    }

    return result;
}


bool uni_hal_i2c_master_transmit(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint8_t *buf, uint16_t buf_len, uint32_t timeout) {
    bool result = false;

    if (uni_hal_i2c_is_inited(ctx)) {
        HAL_StatusTypeDef status = HAL_BUSY;
        uint32_t const start_ms = uni_hal_systick_get_ms();
        while (status == HAL_BUSY && (uni_hal_systick_get_ms() - start_ms) <= timeout) {
            status =
                HAL_I2C_Master_Transmit(_uni_hal_i2c_get_handle_hal(ctx->config.instance), _uni_hal_i2c_shiftaddr(dev_addr), buf, buf_len, timeout);
        }

        result = status == HAL_OK;
    }

    return result;
}


bool uni_hal_i2c_mem_read(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint16_t mem_addr, uni_hal_i2c_memadd_size_e mem_addr_size,
                         uint8_t *buf, uint16_t buf_len, uint32_t timeout) {
    bool result = false;

    if (uni_hal_i2c_is_inited(ctx)) {
        HAL_StatusTypeDef status = HAL_BUSY;
        uint32_t const start_ms = uni_hal_systick_get_ms();
        while (status == HAL_BUSY && (uni_hal_systick_get_ms() - start_ms) <= timeout) {
            status = HAL_I2C_Mem_Read(_uni_hal_i2c_get_handle_hal(ctx->config.instance), _uni_hal_i2c_shiftaddr(dev_addr), mem_addr, _uni_hal_i2c_memadd_size((mem_addr_size)),
                                      buf, buf_len, timeout);
        }

        result = status == HAL_OK;
    }

    return result;
}


bool uni_hal_i2c_mem_write(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint16_t mem_addr, uni_hal_i2c_memadd_size_e mem_addr_size,
                          uint8_t *buf, uint16_t buf_len, uint32_t timeout) {
    bool result = false;

    if (uni_hal_i2c_is_inited(ctx)) {
        HAL_StatusTypeDef status = HAL_BUSY;
        uint32_t const start_ms = uni_hal_systick_get_ms();
        while (status == HAL_BUSY && (uni_hal_systick_get_ms() - start_ms) <= timeout) {
            status = HAL_I2C_Mem_Write(_uni_hal_i2c_get_handle_hal(ctx->config.instance), _uni_hal_i2c_shiftaddr(dev_addr), mem_addr, _uni_hal_i2c_memadd_size((mem_addr_size)),
                                       buf, buf_len, timeout);
        }

        result = status == HAL_OK;
    }

    return result;
}
