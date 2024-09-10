#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//
// Includes
//

// stdlib
#include <stdbool.h>
#include <stdint.h>

// UNI_HAL HAL
#include "gpio/uni_hal_gpio.h"
#include "rcc/uni_hal_rcc.h"



//
// Typedefs
//

typedef enum {
    UNI_HAL_I2C_MEMADD_SIZE_8BIT,
    UNI_HAL_I2C_MEMADD_SIZE_16BIT,
} uni_hal_i2c_memadd_size_e;

typedef enum {
    UNI_HAL_I2C_SPEED_100KHZ,
    UNI_HAL_I2C_SPEED_400KHZ,
    UNI_HAL_I2C_SPEED_1MHZ,
} uni_hal_i2c_speed_e;

typedef struct {
    /**
     * Instance
     */
    uni_hal_core_periph_e instance;

    /**
     * I2C speed
     */
    uni_hal_i2c_speed_e speed;

    /**
     * SCK pin
     */
    uni_hal_gpio_pin_context_t *pin_sck;

    /**
     * SDA pin
     */
    uni_hal_gpio_pin_context_t *pin_sda;
} uni_hal_i2c_config_t;


/**
 * I2C context structure
 */
typedef struct {
    /**
     * Initialization state
     */
    bool initialized;
} uni_hal_i2c_state_t;


/**
 * I2C context
 */
typedef struct {
    /**
     * Config
     */
    uni_hal_i2c_config_t config;

    /**
     * Status
     */
    uni_hal_i2c_state_t  state;
} uni_hal_i2c_context_t;



//
// Functions
//


/**
 * Initializes I2C interface
 * @param ctx pointer to interface context
 * @return true on success
 */
bool uni_hal_i2c_init(uni_hal_i2c_context_t *ctx);

/**
 * Checks taht I2C interface was properly inited
 * @param ctx pointer to the interface context
 * @return true in case of initialized I2C
 */
bool uni_hal_i2c_is_inited(uni_hal_i2c_context_t *ctx);

/**
 * Deinitialize I2C interface
 * @param ctx pointer to the interface context
 * @return true on success
 */
bool uni_hal_i2c_deinit(uni_hal_i2c_context_t *ctx);

/**
 *
 * @param ctx
 * @return
 */
bool uni_hal_i2c_reset(uni_hal_i2c_context_t *ctx);

/**
 * Check if target device is ready for communication
 * @param ctx pointer to interface context
 * @param dev_addr target device address
 * @param trials number of trials before error
 * @param timeout operation timeout in msecs
 * @return true if device is ready
 */
bool uni_hal_i2c_isready(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint32_t trials, uint32_t timeout);


/**
 * Receives data in master blocking mode.
 * @param ctx pointer to interface context
 * @param dev_addr target device address, must be aligned to left
 * @param buf pointer to receive buffer
 * @param buf_len receive buffer length
 * @param timeout operation timeout in msecs
 * @return true on success
 */
bool uni_hal_i2c_master_receive(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint8_t *buf, uint16_t buf_len, uint32_t timeout);


/**
 * Transmits data in master blocking mode.
 * @param ctx pointer to interface context
 * @param dev_addr target device address, must be aligned to left
 * @param buf pointer to transmit buffer
 * @param buf_len transmit buffer length
 * @param timeout operation timeout in msecs
 * @return true on success
 */
bool uni_hal_i2c_master_transmit(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint8_t *buf, uint16_t buf_len, uint32_t timeout);


/**
 * Reads data from specific memory address in blocking mode
 * @param ctx pointer to interface context
 * @param dev_addr target device address
 * @param mem_addr target device memory address
 * @param mem_addr_size target device memory address size in bytes (1 or 2)
 * @param buf pointer to receive buffer
 * @param buf_len receive buffer length
 * @param timeout operation timeout in msecs
 * @return true on success
 */
bool uni_hal_i2c_mem_read(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint16_t mem_addr, uni_hal_i2c_memadd_size_e mem_addr_size,
                         uint8_t *buf, uint16_t buf_len, uint32_t timeout);


/**
 * Writes data to specific memory address in blocking mode
 * @param ctx pointer to interface context
 * @param dev_addr target device address
 * @param mem_addr target device memory address
 * @param mem_addr_size target device memory address size in bytes (1 or 2)
 * @param buf pointer to transmit buffer
 * @param buf_len transmit buffer length
 * @param timeout operation timeout in msecs
 * @return true on success
 */
bool uni_hal_i2c_mem_write(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint16_t mem_addr, uni_hal_i2c_memadd_size_e mem_addr_size,
                          uint8_t *buf, uint16_t buf_len, uint32_t timeout);


#if defined(__cplusplus)
}
#endif
