//
// Includes
//

// stdlib
#include <stddef.h>

// STM
#include <stm32l496xx.h>
#include <stm32l4xx_hal.h>
#include <stm32l4xx_hal_i2c.h>
#include <stm32l4xx_ll_bus.h>
#include <stm32l4xx_ll_i2c.h>
#include <stm32l4xx_ll_rcc.h>

// Uni.Common
#include <uni_common.h>

// Uni.HAL
#include "i2c/uni_hal_i2c.h"
#include "core/uni_hal_core.h"
#include "gpio/uni_hal_gpio.h"
#include "rcc/uni_hal_rcc.h"



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
static uint16_t _uni_hal_i2c_memadd_size(uni_hal_i2c_memadd_size_e size) {
    uint16_t result = 0;
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

static I2C_TypeDef *_uni_hal_i2c_get_handle(uni_hal_core_periph_e instance) {
    I2C_TypeDef *result = NULL;
    switch (instance) {
    case UNI_HAL_CORE_PERIPH_I2C_1:
        result = I2C1;
        break;
    case UNI_HAL_CORE_PERIPH_I2C_2:
        result = I2C2;
        break;
    case UNI_HAL_CORE_PERIPH_I2C_3:
        result = I2C3;
        break;
    case UNI_HAL_CORE_PERIPH_I2C_4:
        result = I2C4;
        break;
    default:
        break;
    }
    return result;
}

static I2C_HandleTypeDef *_uni_hal_i2c_get_handle_hal(uni_hal_core_periph_e instance) {
    I2C_HandleTypeDef *result = NULL;
    switch (instance) {
    case UNI_HAL_CORE_PERIPH_I2C_1:
        result = &hi2c1;
        break;
    case UNI_HAL_CORE_PERIPH_I2C_2:
        result = &hi2c2;
        break;
    case UNI_HAL_CORE_PERIPH_I2C_3:
        result = &hi2c3;
        break;
    case UNI_HAL_CORE_PERIPH_I2C_4:
        result = &hi2c4;
        break;
    default:
        break;
    }
    return result;
}


//
// Public
//

bool uni_hal_i2c_init(uni_hal_i2c_context_t *ctx) {
    bool result = false;

    if (ctx != NULL && !uni_hal_i2c_is_inited(ctx)) {
        I2C_TypeDef *handle = _uni_hal_i2c_get_handle(ctx->config.instance);
        I2C_HandleTypeDef *handle_hal = _uni_hal_i2c_get_handle_hal(ctx->config.instance);
        if (handle != NULL && handle_hal != NULL) {
            result = true;
            if (!ctx->state.initialized) {
                result = uni_hal_rcc_clk_set(ctx->config.instance, true);

                if (!uni_hal_gpio_pin_is_inited(ctx->config.pin_sck)) {
                    result = uni_hal_gpio_pin_init(ctx->config.pin_sck) && result;
                }
                if(!uni_hal_gpio_pin_is_inited(ctx->config.pin_sda)) {
                    result = uni_hal_gpio_pin_init(ctx->config.pin_sda) && result;
                }

                if(result) {
                    handle_hal->Instance = handle;

                    switch(ctx->config.speed) {
                    case UNI_HAL_I2C_SPEED_400KHZ:
                        handle_hal->Init.Timing = 0x00702991;
                        break;
                    case UNI_HAL_I2C_SPEED_1MHZ:
                        handle_hal->Init.Timing = 0x00300F33;
                        break;
                    case UNI_HAL_I2C_SPEED_100KHZ:
                    default:
                        handle_hal->Init.Timing = 0x10909CEC;
                        break;
                    }

                    handle_hal->Init.OwnAddress1 = 0;
                    handle_hal->Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
                    handle_hal->Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
                    handle_hal->Init.OwnAddress2 = 0;
                    handle_hal->Init.OwnAddress2Masks = I2C_OA2_NOMASK;
                    handle_hal->Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
                    handle_hal->Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
                    result = HAL_I2C_Init(handle_hal) == HAL_OK;

                    if(result) {
                        result = HAL_I2CEx_ConfigAnalogFilter(handle_hal, I2C_ANALOGFILTER_ENABLE) == HAL_OK;
                    }

                    if(result){
                        result = HAL_I2CEx_ConfigDigitalFilter(handle_hal, 0) == HAL_OK;
                    }
                }
            }
        }
        ctx->state.initialized = result;
    }

    return result;
}


bool uni_hal_i2c_deinit(uni_hal_i2c_context_t *ctx) {
    bool result = false;
    if (ctx != NULL) {
        I2C_HandleTypeDef *handle_hal = _uni_hal_i2c_get_handle_hal(ctx->config.instance);
        if (handle_hal != NULL) {
            HAL_I2C_DeInit(handle_hal);
            LL_I2C_Disable(handle_hal->Instance);
            LL_I2C_DeInit(handle_hal->Instance);
        }
        ctx->state.initialized = false;
        result = true;
    }
    return result;
}


bool uni_hal_i2c_isready(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint32_t trials, uint32_t timeout) {
    bool result = false;

    if (uni_hal_i2c_is_inited(ctx)) {
        HAL_StatusTypeDef status = HAL_BUSY;
        while (status == HAL_BUSY) {
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
        while (status == HAL_BUSY) {
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
        while (status == HAL_BUSY) {
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
        while (status == HAL_BUSY) {
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
        while (status == HAL_BUSY) {
            status = HAL_I2C_Mem_Write(_uni_hal_i2c_get_handle_hal(ctx->config.instance), _uni_hal_i2c_shiftaddr(dev_addr), mem_addr, _uni_hal_i2c_memadd_size((mem_addr_size)),
                                       buf, buf_len, timeout);
        }

        result = status == HAL_OK;
    }

    return result;
}
