// Model an I2C byte arriving. The IRQ vectors, NVIC, ST event dispatch and FreeRTOS semaphore are real.
#include <stm32l4xx_hal.h>

static I2C_TypeDef regs;
static unsigned starts;
#undef I2C1
#define I2C1 (&regs)

static HAL_StatusTypeDef byte_received(I2C_HandleTypeDef *h, uint32_t flags, uint32_t sources) {
    (void)flags;
    (void)sources;
    *h->pBuffPtr = 0x5a;
    HAL_I2C_MemRxCpltCallback(h);
    return HAL_OK;
}

static HAL_StatusTypeDef start_read(I2C_HandleTypeDef *h, uint16_t addr, uint16_t reg, uint16_t size,
                                    uint8_t *buf, uint16_t len) {
    (void)addr;
    (void)reg;
    (void)size;
    (void)len;
    ++starts;
    if (__get_FAULTMASK()) {
        // Avoid starting an IRQ that cannot run on a broken driver.
        return HAL_ERROR;
    }
    h->pBuffPtr = buf;
    h->XferISR = byte_received;
    NVIC_SetPendingIRQ(I2C1_EV_IRQn);
    return HAL_OK;
}
#define HAL_I2C_Mem_Read_IT start_read
#include "i2c/uni_hal_i2c_stm32l4.c"

_Noreturn void finish(unsigned code);
#define CHECK(condition) do { if (!(condition)) finish(__LINE__); } while (0)

static uni_hal_gpio_pin_context_t scl = {.inited = true}, sda = {.inited = true};
static uni_hal_i2c_context_t ctx = {
    .config = {.instance = UNI_HAL_CORE_PERIPH_I2C_1, .timing = 42U, .pin_sck = &scl, .pin_sda = &sda,
               .irq_enable = true},
};

static void read_byte(void *arg) {
    (void)arg;
    uint8_t byte = 0U;
    __set_FAULTMASK(1U);
    bool const started = uni_hal_i2c_mem_read(&ctx, 0x48U, 0U, UNI_HAL_I2C_MEMADD_SIZE_8BIT, &byte, 1U, 100U);
    uint32_t const mask = __get_FAULTMASK();
    __set_FAULTMASK(0U);
    CHECK(!started && starts == 0U && byte == 0U && mask == 1U);
    CHECK(uni_hal_i2c_mem_read(&ctx, 0x48U, 0U, UNI_HAL_I2C_MEMADD_SIZE_8BIT, &byte, 1U, 100U));
    CHECK(byte == 0x5a);
    finish(0U);
}

int main(void) {
    uint32_t const min_priority = configMAX_SYSCALL_INTERRUPT_PRIORITY >> (8U - __NVIC_PRIO_BITS);
    ctx.config.irq_priority = min_priority;
    NVIC_SetPriorityGrouping(4U); // Subpriority bits are incompatible with the FreeRTOS Cortex-M port.
    CHECK(!uni_hal_i2c_init(&ctx));
    NVIC_SetPriorityGrouping(0U); // All bits are pre-emption bits on both the model and the STM32.
    ctx.config.irq_priority = min_priority - 1U;
    CHECK(!uni_hal_i2c_init(&ctx));
    ctx.config.irq_priority = 1U << __NVIC_PRIO_BITS;
    CHECK(!uni_hal_i2c_init(&ctx));
    CHECK(!ctx.state.initialized && !regs.CR1 && !NVIC_GetEnableIRQ(I2C1_EV_IRQn) && !NVIC_GetEnableIRQ(I2C1_ER_IRQn));

    ctx.config.irq_priority = min_priority;
    CHECK(uni_hal_i2c_init(&ctx));
    CHECK(NVIC_GetPriority(I2C1_EV_IRQn) == min_priority && NVIC_GetPriority(I2C1_ER_IRQn) == min_priority);
    CHECK(xTaskCreate(read_byte, "i2c", 512U, nullptr, 1U, nullptr) == pdPASS);
    vTaskStartScheduler();
    finish(__LINE__);
}
