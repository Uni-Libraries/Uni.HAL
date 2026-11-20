// Run the H743 public entry points against blocking HAL calls with a controlled clock.
#undef NDEBUG
#include <assert.h>
#include <stdint.h>
#include <FreeRTOS.h>
#include <task.h>

#include "i2c/uni_hal_i2c.h"
#include "stm32h7xx_hal.h"
#include "stm32h7xx_ll_i2c.h"

#if defined(TEST_I2C_TASKS_QEMU)
_Noreturn void finish(unsigned code);
#undef assert
#define assert(condition) do { if (!(condition)) finish(__LINE__); } while (0)
#endif

typedef enum { READY, RX, TX, MEM_RX, MEM_TX } operation_t;
static operation_t expected;
static uint32_t now, calls, first_elapsed, waits[2];
static HAL_StatusTypeDef first_status, second_status;
static uint8_t byte;
extern I2C_HandleTypeDef hi2c1, hi2c2;
static uni_hal_i2c_context_t primary, secondary;
#if defined(TEST_I2C_TASKS_QEMU)
static TaskHandle_t owner_task, contender_task;
// Leave time for another scheduler tick if the contender wakes just before the owner's release.
enum { task_timeout_ms = 100U, owner_elapsed_ms = 3U };
#endif
static bool test_ownership, in_owner_hal, advance_clock;
static bool transfer(uni_hal_i2c_context_t *ctx, uint32_t timeout);
I2C_TypeDef model_instances[4];
static uint32_t ipsr, primask, basepri, systick_priority;
uint32_t __get_IPSR(void) { return ipsr; }
uint32_t __get_PRIMASK(void) { return primask; }
uint32_t __get_BASEPRI(void) { return basepri; }
uint32_t NVIC_GetPriority(int irq) { assert(irq == SysTick_IRQn); return systick_priority; }


uint32_t uni_hal_systick_get_ms(void) { return advance_clock ? ++now : now; }

// Preempt between the vendor's availability check and the write that claims the handle.
static HAL_StatusTypeDef ownership_attempt(I2C_HandleTypeDef *handle, uint32_t wait)
{
    if (handle == &hi2c2) {
        assert(in_owner_hal);
        return HAL_OK;
    }
    assert(handle == &hi2c1);
    assert(!in_owner_hal);
#if defined(TEST_I2C_TASKS_QEMU)
    if (xTaskGetCurrentTaskHandle() == owner_task) {
        in_owner_hal = true;
        xTaskNotifyGive(contender_task);
        // The higher-priority contender is blocked waiting for this bus.
        now += owner_elapsed_ms;
        vTaskDelay(1U); // Let the contender retry while the owner is still inside HAL.
        in_owner_hal = false;
    } else {
        assert(wait == task_timeout_ms - owner_elapsed_ms);
    }
#else
    (void)wait;
    in_owner_hal = true;
    assert(transfer(&secondary, 10U));
    advance_clock = true;
    assert(!transfer(&primary, 3U));
    advance_clock = false;
    in_owner_hal = false;
#endif
    return HAL_OK;
}

static HAL_StatusTypeDef attempt(operation_t op, I2C_HandleTypeDef *handle, uint16_t addr, uint32_t wait)
{
    if (test_ownership) {
        assert(op == expected && addr == 0x80);
        return ownership_attempt(handle, wait);
    }
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

static void init_bus(uni_hal_i2c_context_t *ctx, uni_hal_core_periph_e instance, uni_hal_gpio_pin_context_t pins[2])
{
    pins[0].inited = true;
    pins[1].inited = true;
    ctx->config.instance = instance;
    ctx->config.pin_sck = &pins[0];
    ctx->config.pin_sda = &pins[1];
    ctx->config.timing = 42U;
    assert(uni_hal_i2c_init(ctx));
}

#if defined(TEST_I2C_TASKS_QEMU)
static void owner(void *arg)
{
    (void)arg;
    for (expected = READY; expected <= MEM_TX; ++expected) {
        assert(transfer(&primary, 10U));
        assert(ulTaskNotifyTake(pdTRUE, portMAX_DELAY));
    }
    finish(0U);
}

static void contender(void *arg)
{
    (void)arg;
    for (;;) {
        assert(ulTaskNotifyTake(pdTRUE, portMAX_DELAY));
        ipsr = 1U;
        assert(!transfer(&secondary, 10U));
        ipsr = 0U;
        assert(transfer(&secondary, 10U));

        vTaskSuspendAll();
        advance_clock = true;
        assert(!transfer(&primary, 3U));
        advance_clock = false;
        (void)xTaskResumeAll();
        assert(transfer(&primary, task_timeout_ms));
        xTaskNotifyGive(owner_task);
    }
}

int main(void)
{
    static uni_hal_gpio_pin_context_t pins[4];
    init_bus(&primary, UNI_HAL_CORE_PERIPH_I2C_1, &pins[0]);
    init_bus(&secondary, UNI_HAL_CORE_PERIPH_I2C_2, &pins[2]);
    test_ownership = true;
    assert(xTaskCreate(owner, "owner", 512U, NULL, 1U, &owner_task) == pdPASS);
    assert(xTaskCreate(contender, "contender", 512U, NULL, 2U, &contender_task) == pdPASS);
    vTaskStartScheduler();
    finish(__LINE__);
}
#else
int main(int argc, char **argv)
{
    (void)argv;
    uni_hal_gpio_pin_context_t pins[4] = {0};
    init_bus(&primary, UNI_HAL_CORE_PERIPH_I2C_1, &pins[0]);
    if (argc > 1) {
        init_bus(&secondary, UNI_HAL_CORE_PERIPH_I2C_2, &pins[2]);
        test_ownership = true;
        for (expected = READY; expected <= MEM_TX; ++expected) {
            assert(transfer(&primary, 10U));
        }
        return 0;
    }
    uni_hal_i2c_context_t copy = primary;
    for (expected = READY; expected <= MEM_TX; ++expected) {
        assert(!transfer(&copy, 10U) && calls == 0U);
        first_status = HAL_BUSY;
        first_elapsed = 9U;
        second_status = HAL_OK;
        now = UINT32_MAX - 5U;
        assert(transfer(&primary, 10U));
        assert(calls == 2U && waits[0] == 10U && waits[1] == 1U && now == 4U);

        second_status = HAL_ERROR;
        now = 0U;
        assert(!transfer(&primary, 10U) && calls == 2U && now == 10U);

        first_elapsed = 10U;
        assert(!transfer(&primary, 10U) && calls == 1U);
        assert(!transfer(&primary, 0U) && calls == 0U);

        first_status = HAL_ERROR;
        assert(!transfer(&primary, 10U) && calls == 1U);
        first_status = HAL_OK;
        first_elapsed = 11U;
        assert(!transfer(&primary, 10U) && calls == 1U);
        first_elapsed = 0U;
        assert(transfer(&primary, 1U) && calls == 1U && waits[0] == 1U);
        assert(transfer(&primary, UINT32_MAX) && waits[0] != UINT32_MAX);

        basepri = 0x40U;
        systick_priority = 0U; // Bootstrap SysTick can preempt the kernel interrupt mask.
        assert(transfer(&primary, 1U) && calls == 1U);
        systick_priority = 4U;
        assert(!transfer(&primary, 1U) && calls == 0U);
        basepri = 0U;
        primask = 1U;
        assert(!transfer(&primary, 1U) && calls == 0U);
        primask = 0U;
        ipsr = 1U;
        assert(!transfer(&primary, 1U) && calls == 0U);
        ipsr = 0U;
    }
    return 0;
}
#endif
