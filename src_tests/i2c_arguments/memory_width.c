#include <stm32h7xx.h>
#include "i2c/uni_hal_i2c.h"

static I2C_TypeDef registers;
#undef I2C1
#define I2C1 (&registers)

static bool enable_clock(uni_hal_core_periph_e instance, bool state) {
    return instance == UNI_HAL_CORE_PERIPH_I2C_1 && state;
}
#define uni_hal_rcc_clk_set enable_clock
#include "i2c/uni_hal_i2c_stm32h7.c"
#include "../arm/semihost_test.h"

static unsigned starts;
static uint32_t last_start;

uint32_t __wrap_HAL_GetTick(void) {
    static uint32_t tick;
    registers.ISR &= ~registers.ICR;
    registers.ICR = 0;
    if (registers.CR2 & I2C_CR2_START) {
        ++starts;
        last_start = registers.CR2;
        registers.CR2 &= ~I2C_CR2_START;
        registers.ISR = I2C_ISR_NACKF | I2C_ISR_STOPF;
    }
    return tick++;
}

static unsigned check(void) {
    uni_hal_gpio_pin_context_t sda = {.inited = true}, scl = {.inited = true};
    uni_hal_i2c_context_t ctx = {.config = {
        .instance = UNI_HAL_CORE_PERIPH_I2C_1, .pin_sck = &scl, .pin_sda = &sda, .timing = 42U,
    }};
    CHECK(uni_hal_i2c_init(&ctx));

    uint8_t byte = 0xAB;
    for (unsigned read = 0; read < 2; ++read) {
        uni_hal_i2c_memadd_size_e const widths[] = {
            (uni_hal_i2c_memadd_size_e)2, UNI_HAL_I2C_MEMADD_SIZE_8BIT, UNI_HAL_I2C_MEMADD_SIZE_16BIT,
        };
        for (unsigned i = 0; i < 3; ++i) {
            starts = 0;
            bool ok = read ? uni_hal_i2c_mem_read(&ctx, 0x50, 0x1234, widths[i], &byte, 1, 20)
                           : uni_hal_i2c_mem_write(&ctx, 0x50, 0x1234, widths[i], &byte, 1, 20);
            CHECK(!ok && byte == 0xAB);
            if (i == 0) {
                CHECK(starts == 0);
            } else {
                // For valid widths the addressed device NACKs the register phase.
                CHECK(starts == 1 && ((last_start & I2C_CR2_NBYTES) >> I2C_CR2_NBYTES_Pos) == i);
            }
        }
    }
    return 0;
}
