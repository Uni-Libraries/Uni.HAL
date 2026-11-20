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
// Defines
//

#define UNI_HAL_I2C_MAXINSTANCES (4U)



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

#if defined(UNI_HAL_TARGET_MCU_PC)
typedef enum {
    UNI_HAL_I2C_PC_PROBE,
    UNI_HAL_I2C_PC_MASTER_RECEIVE,
    UNI_HAL_I2C_PC_MASTER_TRANSMIT,
    UNI_HAL_I2C_PC_MEM_READ,
    UNI_HAL_I2C_PC_MEM_WRITE,
} uni_hal_i2c_pc_operation_e;

typedef struct {
    uni_hal_i2c_pc_operation_e operation;
    uint16_t dev_addr; /**< Normalized 7-bit device address. */
    uint16_t mem_addr;
    uni_hal_i2c_memadd_size_e mem_addr_size;
    uint8_t *buf; /**< Fill on reads, inspect on writes. */
    uint16_t buf_len;
    uint32_t timeout; /**< Total budget in milliseconds. */
    uint32_t trials;  /**< Probe attempt budget, zero for data transfers. */
} uni_hal_i2c_pc_request_t;

/**
 * Synchronous model of one bus. Return the byte count transferred, zero for an
 * acknowledged probe, or a negative value on failure. Fill every counted byte
 * on a read and return within the requested timeout. The request and buffer
 * may only be used during the call.
 */
typedef int32_t (*uni_hal_i2c_pc_exchange_fn)(void *user, const uni_hal_i2c_pc_request_t *request);
#endif

typedef struct uni_hal_i2c_config_t {
    /**
     * Instance
     */
    uni_hal_core_periph_e instance;

    /**
     * I2C kernel clock source. On STM32L496, select PCLK1, SYSCLK or HSI.
     * On STM32H743, I2C1..3 share PCLK1/PLL3R/HSI/CSI and I2C4 uses PCLK4/PLL3R/HSI/CSI.
     * A conflicting source fails initialization while a sibling uses the shared clock.
     * UNKNOWN preserves the existing selection (PCLK1 after L496 RCC initialization).
     */
    uni_hal_rcc_clksrc_e clock_source;

    /**
     * I2C speed
     */
    uni_hal_i2c_speed_e speed;

    /**
     * Raw value of the I2C_TIMINGR register, for a bus that needs a timing none of the standard
     * speeds gives. 0 means: compute the timing for `speed` from the kernel clock.
     */
    uint32_t timing;

    /**
     * SCK pin
     */
    uni_hal_gpio_pin_context_t *pin_sck;

    /**
     * SDA pin
     */
    uni_hal_gpio_pin_context_t *pin_sda;

    /**
     * Use interrupts
     */
    bool irq_enable;

    /**
     * Unshifted NVIC pre-emption priority. STM32L496 interrupt transfers require a
     * FreeRTOS syscall-safe priority (4..15 with the default configuration) and no
     * subpriority bits. Initialization rejects unsupported interrupt settings.
     */
    uint32_t irq_priority;

#if defined(UNI_HAL_TARGET_MCU_PC)
    /** Optional bus model. An empty bus acknowledges no device. */
#ifdef __cplusplus
    uni_hal_i2c_pc_exchange_fn pc_exchange = nullptr;
    void *pc_user = nullptr;
#else
    uni_hal_i2c_pc_exchange_fn pc_exchange;
    void *pc_user;
#endif
#endif
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

/*
 * Device addresses
 *
 * Every function takes the plain 7-bit address of the device, as data sheets give it, and
 * shifts it into the address byte itself. A value of 0x80 or above cannot be a 7-bit address
 * and is taken as an address byte that is already shifted left (e.g. 0xA0 for the device 0x50).
 * A shifted address below 0x80 cannot be told apart from a 7-bit one, so never pass those.
 */


/**
 * Initialize the I2C interface. On STM32, supply both SCL and SDA pin contexts.
 * Serialize init, deinit, reset and recovery for the same physical bus, and stop transfers
 * before reconfiguring it. Recovery temporarily takes both pins away from the peripheral.
 * @param ctx pointer to interface context
 * @return true on success
 */
bool uni_hal_i2c_init(uni_hal_i2c_context_t *ctx);

/**
 * Compute the I2C_TIMINGR value of the STM32 I2C peripheral (analog filter on, digital filter off)
 * @param clock_hz frequency of the I2C kernel clock
 * @param speed bus speed
 * @return register value; 0 when the clock is too slow for this speed
 * @note the bus never runs faster than `speed`; it runs slower by what the SCL edges take
 */
uint32_t uni_hal_i2c_timing_calc(uint32_t clock_hz, uni_hal_i2c_speed_e speed);

/**
 * Checks taht I2C interface was properly inited
 * @param ctx pointer to the interface context
 * @return true in case of initialized I2C
 */
bool uni_hal_i2c_is_inited(uni_hal_i2c_context_t *ctx);

/**
 * Deinitialize I2C interface from task or startup context.
 * On STM32L496 and STM32H743 this also resets a valid peripheral before its first initialization.
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
 * Free a bus that a slave holds: SDA stuck low after a transfer was cut short, e.g. by a reset
 * of the master. The peripheral is shut down, up to nine clocks and a STOP condition are sent
 * on the pins as GPIO, then the interface is initialised again.
 * GPIO signalling takes up to about 1.1 ms, plus at most 1 ms of waiting for SCL to rise.
 * @param ctx pointer to the interface context
 * @return true when a STOP was issued, both lines are high and the interface is initialised again
 */
bool uni_hal_i2c_recover(uni_hal_i2c_context_t *ctx);

/**
 * Check if target device is ready for communication
 * @param ctx pointer to interface context
 * @param dev_addr 7-bit address of the target device (0x00..0x7F), not shifted; see the note on device addresses
 * @param trials number of trials before error
 * @param timeout total operation timeout in milliseconds; on STM32 zero starts no transfer
 * @return true if device is ready
 */
bool uni_hal_i2c_isready(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint32_t trials, uint32_t timeout);


/**
 * Receives data in master blocking mode.
 * @param ctx pointer to interface context
 * @param dev_addr 7-bit address of the target device (0x00..0x7F), not shifted; see the note on device addresses
 * @param buf pointer to receive buffer
 * @param buf_len receive buffer length
 * @param timeout total operation timeout in milliseconds; on STM32 zero starts no transfer
 * @return true on success
 */
bool uni_hal_i2c_master_receive(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint8_t *buf, uint16_t buf_len, uint32_t timeout);


/**
 * Transmits data in master blocking mode.
 * @param ctx pointer to interface context
 * @param dev_addr 7-bit address of the target device (0x00..0x7F), not shifted; see the note on device addresses
 * @param buf pointer to transmit buffer
 * @param buf_len transmit buffer length
 * @param timeout total operation timeout in milliseconds; on STM32 zero starts no transfer
 * @return true on success
 */
bool uni_hal_i2c_master_transmit(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint8_t *buf, uint16_t buf_len, uint32_t timeout);


/**
 * Reads data from specific memory address in blocking mode. On STM32L496 the buffer
 * is released before return on success or failure. A zero timeout starts no transfer.
 * @param ctx pointer to interface context
 * @param dev_addr 7-bit address of the target device (0x00..0x7F), not shifted; see the note on device addresses
 * @param mem_addr target device memory address
 * @param mem_addr_size target device memory address size in bytes (1 or 2)
 * @param buf pointer to receive buffer
 * @param buf_len receive buffer length
 * @param timeout total operation timeout in milliseconds; on STM32 zero starts no transfer
 * @return true on success
 */
bool uni_hal_i2c_mem_read(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint16_t mem_addr, uni_hal_i2c_memadd_size_e mem_addr_size,
                         uint8_t *buf, uint16_t buf_len, uint32_t timeout);


/**
 * Writes data to specific memory address in blocking mode. On STM32L496 the buffer
 * is released before return on success or failure. A zero timeout starts no transfer.
 * @param ctx pointer to interface context
 * @param dev_addr 7-bit address of the target device (0x00..0x7F), not shifted; see the note on device addresses
 * @param mem_addr target device memory address
 * @param mem_addr_size target device memory address size in bytes (1 or 2)
 * @param buf pointer to transmit buffer
 * @param buf_len transmit buffer length
 * @param timeout total operation timeout in milliseconds; on STM32 zero starts no transfer
 * @return true on success
 */
bool uni_hal_i2c_mem_write(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint16_t mem_addr, uni_hal_i2c_memadd_size_e mem_addr_size,
                          uint8_t *buf, uint16_t buf_len, uint32_t timeout);


#if defined(__cplusplus)
}
#endif
