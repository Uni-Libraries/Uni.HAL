// Run the H743 public entry points against blocking HAL calls with a controlled clock.
#undef NDEBUG
#include <assert.h>
#include <stdint.h>

#include "i2c/uni_hal_i2c.h"
#include "stm32h7xx_hal.h"
#include "stm32h7xx_ll_i2c.h"

typedef enum { READY, RX, TX, MEM_RX, MEM_TX } operation_t;
static operation_t expected;
static uint32_t now, calls, first_elapsed, waits[2];
static HAL_StatusTypeDef first_status, second_status;
static uint8_t byte;
extern I2C_HandleTypeDef hi2c1;
I2C_TypeDef model_instances[4];
static uint32_t ipsr, primask, basepri, systick_priority;
uint32_t __get_IPSR(void) { return ipsr; }
uint32_t __get_PRIMASK(void) { return primask; }
uint32_t __get_BASEPRI(void) { return basepri; }
uint32_t NVIC_GetPriority(int irq) { assert(irq == SysTick_IRQn); return systick_priority; }


uint32_t uni_hal_systick_get_ms(void) { return now; }

static HAL_StatusTypeDef attempt(operation_t op, I2C_HandleTypeDef *handle, uint16_t addr, uint32_t wait)
{
    assert(op == expected && handle == &hi2c1 && addr == 0x80 && calls < 2U);
    waits[calls++] = wait;
    if (calls == 1U) {
        now += first_elapsed;
        return first_status;
    }
    now += wait;
    return second_status;
}

HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *h, uint16_t addr, uint32_t trials, uint32_t wait)
{
    assert(trials == 3U);
    return attempt(READY, h, addr, wait);
}
HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef *h, uint16_t addr, uint8_t *data, uint16_t len, uint32_t wait)
{
    assert(data == &byte && len == 1U);
    return attempt(RX, h, addr, wait);
}
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *h, uint16_t addr, uint8_t *data, uint16_t len, uint32_t wait)
{
    assert(data == &byte && len == 1U);
    return attempt(TX, h, addr, wait);
}
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *h, uint16_t addr, uint16_t reg, uint16_t size, uint8_t *data, uint16_t len, uint32_t wait)
{
    assert(reg == 7U && size == I2C_MEMADD_SIZE_16BIT && data == &byte && len == 1U);
    return attempt(MEM_RX, h, addr, wait);
}
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *h, uint16_t addr, uint16_t reg, uint16_t size, uint8_t *data, uint16_t len, uint32_t wait)
{
    assert(reg == 7U && size == I2C_MEMADD_SIZE_16BIT && data == &byte && len == 1U);
    return attempt(MEM_TX, h, addr, wait);
}

HAL_StatusTypeDef HAL_I2C_Init(I2C_HandleTypeDef *h) { (void)h; return HAL_OK; }
HAL_StatusTypeDef HAL_I2C_DeInit(I2C_HandleTypeDef *h) { (void)h; return HAL_OK; }
HAL_StatusTypeDef HAL_I2CEx_ConfigAnalogFilter(I2C_HandleTypeDef *h, uint32_t v) { (void)h; (void)v; return HAL_OK; }
HAL_StatusTypeDef HAL_I2CEx_ConfigDigitalFilter(I2C_HandleTypeDef *h, uint32_t v) { (void)h; (void)v; return HAL_OK; }
ErrorStatus LL_I2C_DeInit(const I2C_TypeDef *dev) { (void)dev; return SUCCESS; }

static bool transfer(uni_hal_i2c_context_t *ctx, uint32_t timeout)
{
    calls = 0;
    switch (expected) {
        case READY:  return uni_hal_i2c_isready(ctx, 0x40, 3, timeout);
        case RX:     return uni_hal_i2c_master_receive(ctx, 0x40, &byte, 1, timeout);
        case TX:     return uni_hal_i2c_master_transmit(ctx, 0x40, &byte, 1, timeout);
        case MEM_RX: return uni_hal_i2c_mem_read(ctx, 0x40, 7, UNI_HAL_I2C_MEMADD_SIZE_16BIT, &byte, 1, timeout);
        case MEM_TX: return uni_hal_i2c_mem_write(ctx, 0x40, 7, UNI_HAL_I2C_MEMADD_SIZE_16BIT, &byte, 1, timeout);
    }
    return false;
}

int main(void)
{
    uni_hal_i2c_context_t ctx = {.config = {.instance = UNI_HAL_CORE_PERIPH_I2C_1}, .state = {.initialized = true}};
    for (expected = READY; expected <= MEM_TX; ++expected) {
        first_status = HAL_BUSY;
        first_elapsed = 9U;
        second_status = HAL_OK;
        now = UINT32_MAX - 5U;
        assert(transfer(&ctx, 10U));
        assert(calls == 2U && waits[0] == 10U && waits[1] == 1U && now == 4U);

        second_status = HAL_ERROR;
        now = 0U;
        assert(!transfer(&ctx, 10U) && calls == 2U && now == 10U);

        first_elapsed = 10U;
        assert(!transfer(&ctx, 10U) && calls == 1U);
        assert(!transfer(&ctx, 0U) && calls == 0U);

        first_status = HAL_ERROR;
        assert(!transfer(&ctx, 10U) && calls == 1U);
        first_status = HAL_OK;
        first_elapsed = 11U;
        assert(!transfer(&ctx, 10U) && calls == 1U);
        first_elapsed = 0U;
        assert(transfer(&ctx, 1U) && calls == 1U && waits[0] == 1U);
        assert(transfer(&ctx, UINT32_MAX) && waits[0] != UINT32_MAX);

        basepri = 0x40U;
        systick_priority = 0U; // Bootstrap SysTick can preempt the kernel interrupt mask.
        assert(transfer(&ctx, 1U) && calls == 1U);
        systick_priority = 4U;
        assert(!transfer(&ctx, 1U) && calls == 0U);
        basepri = 0U;
        primask = 1U;
        assert(!transfer(&ctx, 1U) && calls == 0U);
        primask = 0U;
        ipsr = 1U;
        assert(!transfer(&ctx, 1U) && calls == 0U);
        ipsr = 0U;
    }
    return 0;
}
