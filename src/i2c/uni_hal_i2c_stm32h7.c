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
#include "core/uni_hal_core.h"
#include "i2c/uni_hal_i2c.h"
#include "uni_hal_i2c_stm32_poll.h"



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

typedef struct {
    I2C_TypeDef *instance;
    I2C_HandleTypeDef *hal;
    uni_hal_i2c_context_t *owner;
    bool busy;
} i2c_bus_t;

static i2c_bus_t buses[] = {
    {I2C1, &hi2c1, NULL, false},
    {I2C2, &hi2c2, NULL, false},
    {I2C3, &hi2c3, NULL, false},
    {I2C4, &hi2c4, NULL, false},
};

static i2c_bus_t *get_bus(uni_hal_core_periph_e instance) {
    if (instance < UNI_HAL_CORE_PERIPH_I2C_1 || instance > UNI_HAL_CORE_PERIPH_I2C_4) {
        return NULL;
    }
    return &buses[instance - UNI_HAL_CORE_PERIPH_I2C_1];
}

static bool claim_init(i2c_bus_t *bus, uni_hal_i2c_context_t *ctx) {
    // Reserve the current selection before GPIO or HAL setup can be preempted by a sibling.
    uint32_t const mask = uni_hal_core_irq_pause();
    bool claimed = !bus->owner;
    uni_hal_rcc_clksrc_e const source = ctx->config.clock_source;
    if (claimed && source != UNI_HAL_RCC_CLKSRC_UNKNOWN) {
        bool const shared_in_use = bus->instance != I2C4 && (buses[0].owner || buses[1].owner || buses[2].owner);
        claimed = shared_in_use ? uni_hal_rcc_clksrc_get(ctx->config.instance) == source
                                : uni_hal_rcc_clksrc_set(ctx->config.instance, source);
    }
    if (claimed) {
        bus->owner = ctx;
    }
    uni_hal_core_irq_resume(mask);
    return claimed;
}

static i2c_bus_t *claim_transfer(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint32_t start, uint32_t timeout) {
    if (dev_addr > UINT8_MAX || !timeout || !polling_allowed() || !uni_hal_i2c_is_inited(ctx)) {
        return NULL;
    }
    i2c_bus_t *bus = get_bus(ctx->config.instance);
    if (!bus || bus->owner != ctx) {
        return NULL;
    }
    // The ST handle lock uses separate reads and writes. Reserve before entering HAL.
    do {
        uint32_t mask = uni_hal_core_irq_pause();
        bool claimed = !bus->busy;
        if (claimed) {
            bus->busy = true;
        }
        uni_hal_core_irq_resume(mask);
        if (claimed) {
            return bus;
        }
    } while (retry_budget(start, timeout));
    return NULL;
}

static void release_bus(i2c_bus_t *bus) {
    uint32_t mask = uni_hal_core_irq_pause();
    bus->busy = false;
    uni_hal_core_irq_resume(mask);
}



//
// Public
//

bool uni_hal_i2c_init(uni_hal_i2c_context_t *ctx) {
    i2c_bus_t *bus = ctx ? get_bus(ctx->config.instance) : NULL;
    if (!bus || ctx->state.initialized || !ctx->config.pin_sck || !ctx->config.pin_sda || !claim_init(bus, ctx)) {
        return false;
    }

    if (!uni_hal_rcc_clk_set(ctx->config.instance, true) ||
        (!uni_hal_gpio_pin_is_inited(ctx->config.pin_sck) && !uni_hal_gpio_pin_init(ctx->config.pin_sck)) ||
        (!uni_hal_gpio_pin_is_inited(ctx->config.pin_sda) && !uni_hal_gpio_pin_init(ctx->config.pin_sda))) {
        bus->owner = NULL;
        return false;
    }

    I2C_HandleTypeDef *hal = bus->hal;
    hal->Init.Timing = ctx->config.timing != 0U ? ctx->config.timing
            : uni_hal_i2c_timing_calc(uni_hal_rcc_clk_get_freq(ctx->config.instance), ctx->config.speed);
    if (hal->Init.Timing == 0U) {
        bus->owner = NULL;
        return false;
    }
    hal->Init.OwnAddress1 = 0;
    hal->Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hal->Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hal->Init.OwnAddress2 = 0;
    hal->Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hal->Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hal->Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

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
    i2c_bus_t *bus = ctx ? get_bus(ctx->config.instance) : NULL;
    if (!bus || (bus->owner && bus->owner != ctx)) {
        return false;
    }
    ctx->state.initialized = false;
    if (bus->hal->Instance) {
        if (bus->hal->Instance != bus->instance || HAL_I2C_DeInit(bus->hal) != HAL_OK) {
            return false;
        }
        bus->hal->Instance = NULL;
    }
    // A bootloader can leave the peripheral enabled even when our HAL handle is still empty.
    if (LL_I2C_DeInit(bus->instance) != SUCCESS) {
        return false;
    }
    bus->owner = NULL;
    return true;
}


bool uni_hal_i2c_isready(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint32_t trials, uint32_t timeout) {
    uint32_t start = uni_hal_systick_get_ms();
    i2c_bus_t *bus = claim_transfer(ctx, dev_addr, start, timeout);
    if (!bus) {
        return false;
    }
    bool result = poll_ready(bus->hal, address_byte(dev_addr), trials, start, timeout);
    release_bus(bus);
    return result;
}

static bool transfer_master(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint8_t *buf, uint16_t len,
                            bool read, uint32_t timeout) {
    uint32_t start = uni_hal_systick_get_ms();
    i2c_bus_t *bus = claim_transfer(ctx, dev_addr, start, timeout);
    if (!bus) {
        return false;
    }
    bool result = poll_master(bus->hal, address_byte(dev_addr), buf, len, read, start, timeout);
    release_bus(bus);
    return result;
}

bool uni_hal_i2c_master_receive(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint8_t *buf, uint16_t buf_len, uint32_t timeout) {
    return transfer_master(ctx, dev_addr, buf, buf_len, true, timeout);
}

bool uni_hal_i2c_master_transmit(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint8_t *buf, uint16_t buf_len, uint32_t timeout) {
    return transfer_master(ctx, dev_addr, buf, buf_len, false, timeout);
}

static bool poll_memory(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint16_t mem_addr,
                        uni_hal_i2c_memadd_size_e mem_addr_size, uint8_t *buf, uint16_t len,
                        bool read, uint32_t timeout) {
    uint16_t const size = _uni_hal_i2c_memadd_size(mem_addr_size);
    if (size == 0U) {
        return false;
    }
    uint32_t start = uni_hal_systick_get_ms();
    i2c_bus_t *bus = claim_transfer(ctx, dev_addr, start, timeout);
    if (!bus) {
        return false;
    }
    I2C_HandleTypeDef *handle = bus->hal;
    uint16_t addr = address_byte(dev_addr);
    bool result = false;
    for (uint32_t remaining = remaining_ms(start, timeout); remaining; remaining = retry_budget(start, timeout)) {
        uint32_t wait = hal_timeout(remaining);
        HAL_StatusTypeDef status = read ? HAL_I2C_Mem_Read(handle, addr, mem_addr, size, buf, len, wait)
                                        : HAL_I2C_Mem_Write(handle, addr, mem_addr, size, buf, len, wait);
        if (status != HAL_BUSY) {
            result = finished_in_time(status, start, timeout);
            break;
        }
    }
    release_bus(bus);
    return result;
}

bool uni_hal_i2c_mem_read(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint16_t mem_addr, uni_hal_i2c_memadd_size_e mem_addr_size,
                          uint8_t *buf, uint16_t buf_len, uint32_t timeout) {
    return poll_memory(ctx, dev_addr, mem_addr, mem_addr_size, buf, buf_len, true, timeout);
}

bool uni_hal_i2c_mem_write(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint16_t mem_addr, uni_hal_i2c_memadd_size_e mem_addr_size,
                           uint8_t *buf, uint16_t buf_len, uint32_t timeout) {
    return poll_memory(ctx, dev_addr, mem_addr, mem_addr_size, buf, buf_len, false, timeout);
}
