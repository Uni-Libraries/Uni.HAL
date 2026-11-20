#pragma once

#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include <stm32h7xx.h>
#else
#include <stm32l4xx.h>
#endif
#include "i2c/uni_hal_i2c.h"

static I2C_TypeDef registers;
#undef I2C1
#define I2C1 (&registers)

static bool enable_clock(uni_hal_core_periph_e instance, bool state) {
    return instance == UNI_HAL_CORE_PERIPH_I2C_1 && state;
}
#define uni_hal_rcc_clk_set enable_clock
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include "i2c/uni_hal_i2c_stm32h7.c"
#else
#include "i2c/uni_hal_i2c_stm32l4.c"
#endif
#include "../arm/semihost_test.h"

static unsigned starts;
static uint32_t last_start;
static int responding_address = -1;

uint32_t __wrap_HAL_GetTick(void) {
    static uint32_t tick;
    registers.ISR &= ~registers.ICR;
    registers.ICR = 0;
    if (registers.CR2 & I2C_CR2_START) {
        ++starts;
        last_start = registers.CR2;
        registers.CR2 &= ~I2C_CR2_START;
        int const address = (int)((last_start & 0xFEU) >> 1U);
        registers.ISR = (address == responding_address) ? I2C_ISR_STOPF : (I2C_ISR_NACKF | I2C_ISR_STOPF);
    }
    return tick++;
}

static bool init_bus(uni_hal_i2c_context_t *ctx) {
    static uni_hal_gpio_pin_context_t sda = {.inited = true}, scl = {.inited = true};
    ctx->config.instance = UNI_HAL_CORE_PERIPH_I2C_1;
    ctx->config.pin_sck = &scl;
    ctx->config.pin_sda = &sda;
    ctx->config.timing = 42U;
    return uni_hal_i2c_init(ctx);
}
