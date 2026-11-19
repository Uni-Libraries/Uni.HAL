// Run the public L496 operations with static buffers and a controllable event IRQ.
#undef NDEBUG
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "i2c/uni_hal_i2c.h"
#include "systick/uni_hal_systick.h"
#include "stm32l4xx_hal.h"
#include "stm32l4xx_ll_i2c.h"
#include "task.h"
#include "semphr.h"

extern I2C_HandleTypeDef hi2c1;
void I2C1_EV_IRQHandler(void);

I2C_TypeDef model_instances[4];
static uni_hal_gpio_pin_context_t pins[2];
static uni_hal_i2c_context_t ctx = {
    .config = {.instance = UNI_HAL_CORE_PERIPH_I2C_1, .timing = 42, .pin_sck = &pins[0],
               .pin_sda = &pins[1], .irq_enable = true},
};
static uint8_t buffer[1], competitor_buffer[1];
static uint32_t now, irq_mask, irq_starts, resets, accesses, polls, scheduled_at, waits, delays;
static bool deliver, fail, compete, read_direction, fail_init, fail_start;
static bool complete_at_deadline;
static bool scheduler_running;
static bool in_irq;
static SemaphoreHandle_t active_event;
static uint32_t event_creation_ms;
#if !configSUPPORT_STATIC_ALLOCATION
static bool fail_event_allocation;
#endif

static void run_irq(void)
{
    in_irq = true;
    I2C1_EV_IRQHandler();
    in_irq = false;
}

static void advance(void)
{
    ++now;
    if (irq_mask || !hi2c1.XferISR) {
        return;
    }
    if (compete) {
        compete = false;
        // The fake clock advances on each read. Leave enough time for a tick of sleep and a retry.
        assert(!uni_hal_i2c_mem_write(&ctx, 0x40, 7, UNI_HAL_I2C_MEMADD_SIZE_8BIT, competitor_buffer, 1, 4));
        assert(!uni_hal_i2c_deinit(&ctx));
    }
    if (deliver && now == scheduled_at) {
        run_irq();
    }
}

uint32_t uni_hal_systick_get_ms(void) { advance(); return now; }
uint32_t uni_hal_core_irq_pause(void) {
    uint32_t prior = irq_mask;
    if (!prior && complete_at_deadline && hi2c1.XferISR) {
        // STOPF arrives after the wait expires but just before cancellation masks IRQs.
        complete_at_deadline = false;
        run_irq();
    }
    irq_mask = 1;
    return prior;
}
void uni_hal_core_irq_resume(uint32_t mask) { irq_mask = mask; }
uint32_t __get_PRIMASK(void) { return irq_mask; }
uint32_t __get_BASEPRI(void) { return 0; }
uint32_t NVIC_GetPriority(int irq) { assert(irq == SysTick_IRQn); return 15U; }
uint32_t __get_IPSR(void) { return in_irq; }

bool uni_hal_i2c_is_inited(uni_hal_i2c_context_t *c) { return c && c->state.initialized; }
bool uni_hal_gpio_pin_init(uni_hal_gpio_pin_context_t *pin) { (void)pin; return true; }
bool uni_hal_gpio_pin_is_inited(const uni_hal_gpio_pin_context_t *pin) { (void)pin; return true; }
bool uni_hal_rcc_clk_set(uni_hal_core_periph_e instance, bool enable) { (void)instance; return enable; }
uint32_t uni_hal_rcc_clk_get_freq(uni_hal_core_periph_e instance) { (void)instance; return 0; }
uint32_t uni_hal_i2c_timing_calc(uint32_t hz, uni_hal_i2c_speed_e speed) { (void)hz; (void)speed; return 0; }
bool uni_hal_core_irq_enable(uni_hal_core_irq_e irq, uint32_t priority, uint32_t sub) {
    (void)irq; (void)priority; (void)sub; return true;
}

int xTaskGetSchedulerState(void) { return scheduler_running ? taskSCHEDULER_RUNNING : 0; }
void vTaskDelay(TickType_t ticks) {
    assert(!irq_mask && scheduler_running);
    ++delays;
    while (ticks--) { advance(); }
}
static SemaphoreHandle_t create_event(SemaphoreHandle_t storage) {
    assert(!active_event && scheduler_running && !in_irq && !irq_mask);
    now += event_creation_ms;
    active_event = storage;
    storage->count = 0;
    return storage;
}
#if configSUPPORT_STATIC_ALLOCATION
SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t *storage) {
    return create_event(storage);
}
#else
SemaphoreHandle_t xSemaphoreCreateBinary(void) {
    static ModelSemaphore allocated;
    return fail_event_allocation ? nullptr : create_event(&allocated);
}
#endif
void vSemaphoreDelete(SemaphoreHandle_t event) {
    assert(event && event == active_event && !in_irq);
    active_event = nullptr;
}
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t event, BaseType_t *woken) {
    assert(event == active_event && in_irq && !event->count);
    event->count = 1;
    *woken = pdTRUE;
    return pdTRUE;
}
BaseType_t xSemaphoreTake(SemaphoreHandle_t event, TickType_t ticks) {
    assert(event == active_event && !irq_mask && !in_irq && scheduler_running);
    if (ticks) { ++waits; }
    while (!event->count && ticks--) { advance(); }
    if (!event->count) { return pdFALSE; }
    event->count = 0;
    return pdTRUE;
}

HAL_StatusTypeDef HAL_I2C_Init(I2C_HandleTypeDef *h) {
    assert(h->Init.Timing == 42);
    h->State = HAL_I2C_STATE_READY;
    return fail_init ? HAL_ERROR : HAL_OK;
}
HAL_StatusTypeDef HAL_I2C_DeInit(I2C_HandleTypeDef *h) { h->State = 0; return HAL_OK; }
int LL_I2C_DeInit(I2C_TypeDef *instance) { assert(instance == I2C1); ++resets; return SUCCESS; }
uint32_t HAL_I2C_GetState(I2C_HandleTypeDef *h) { return h->State; }
HAL_StatusTypeDef HAL_I2CEx_ConfigAnalogFilter(I2C_HandleTypeDef *h, uint32_t v) { (void)h; (void)v; return HAL_OK; }
HAL_StatusTypeDef HAL_I2CEx_ConfigDigitalFilter(I2C_HandleTypeDef *h, uint32_t v) { (void)h; (void)v; return HAL_OK; }
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *h, uint16_t a, uint32_t n, uint32_t t) {
    (void)h; (void)a; (void)n; (void)t; return HAL_OK;
}
HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef *h, uint16_t a, uint8_t *b, uint16_t n, uint32_t t) {
    (void)h; (void)a; (void)b; (void)n; (void)t; return HAL_OK;
}
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *h, uint16_t a, uint8_t *b, uint16_t n, uint32_t t) {
    (void)h; (void)a; (void)b; (void)n; (void)t; return HAL_OK;
}

static void transfer_irq(I2C_HandleTypeDef *h)
{
    assert(h->pBuffPtr == buffer && h->State == HAL_I2C_STATE_BUSY);
    h->XferISR = nullptr;
    h->State = HAL_I2C_STATE_READY;
    if (fail) {
        HAL_I2C_ErrorCallback(h);
    } else {
        ++accesses;
        if (read_direction) {
            h->pBuffPtr[0] = 0x3C;
            HAL_I2C_MemRxCpltCallback(h);
        } else {
            assert(h->pBuffPtr[0] == 0x3C);
            HAL_I2C_MemTxCpltCallback(h);
        }
    }
}
void HAL_I2C_EV_IRQHandler(I2C_HandleTypeDef *h) { if (h->XferISR) { h->XferISR(h); } }
void HAL_I2C_ER_IRQHandler(I2C_HandleTypeDef *h) { HAL_I2C_ErrorCallback(h); }

static HAL_StatusTypeDef start(I2C_HandleTypeDef *h, uint16_t addr, uint16_t reg, uint16_t width,
                               uint8_t *b, uint16_t len, bool read)
{
    assert(h == &hi2c1 && !h->XferISR && b == buffer && len == 1);
    assert(addr == 0x80 && reg == 7 && width == I2C_MEMADD_SIZE_8BIT);
    if (fail_start) { return HAL_BUSY; }
    ++irq_starts;
    scheduled_at = now + 7;
    read_direction = read;
    h->pBuffPtr = b;
    h->XferISR = transfer_irq;
    h->State = HAL_I2C_STATE_BUSY;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_I2C_Mem_Read_IT(I2C_HandleTypeDef *h, uint16_t a, uint16_t m, uint16_t w, uint8_t *b, uint16_t n) {
    return start(h, a, m, w, b, n, true);
}
HAL_StatusTypeDef HAL_I2C_Mem_Write_IT(I2C_HandleTypeDef *h, uint16_t a, uint16_t m, uint16_t w, uint8_t *b, uint16_t n) {
    return start(h, a, m, w, b, n, false);
}
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *h, uint16_t a, uint16_t m, uint16_t w, uint8_t *b, uint16_t n, uint32_t t) {
    assert(h && a == 0x80 && m == 7 && w == 1 && t > 0 && t <= 20 && n == 1);
    ++polls; b[0] = 0x3C; return HAL_OK;
}
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *h, uint16_t a, uint16_t m, uint16_t w, uint8_t *b, uint16_t n, uint32_t t) {
    assert(h && a == 0x80 && m == 7 && w == 1 && t > 0 && t <= 20 && n == 1 && b[0] == 0x3C);
    ++polls; return HAL_OK;
}

typedef bool (*operation_t)(uni_hal_i2c_context_t *, uint16_t, uint16_t, uni_hal_i2c_memadd_size_e,
                            uint8_t *, uint16_t, uint32_t);
static void exercise(operation_t op)
{
    uint32_t initial_starts = irq_starts;
    assert(!op(&ctx, 0x40, 7, UNI_HAL_I2C_MEMADD_SIZE_8BIT, buffer, 1, 0));
    assert(irq_starts == initial_starts);

    fail_start = true;
    assert(!op(&ctx, 0x40, 7, UNI_HAL_I2C_MEMADD_SIZE_8BIT, buffer, 1, 20));
    fail_start = false;

    uint32_t initial_resets = resets, initial_accesses = accesses;
    deliver = false;
    now = UINT32_MAX - 3; // SysTick wraps while this transfer is waiting.
    assert(!op(&ctx, 0x40, 7, UNI_HAL_I2C_MEMADD_SIZE_8BIT, buffer, 1, 10));
    assert(resets == initial_resets + 1 && now < 10 && ctx.state.initialized);
    assert(!hi2c1.pBuffPtr && !hi2c1.XferISR);
    run_irq(); // An already pending vector after return.
    assert(accesses == initial_accesses);

    deliver = true;
    fail = true;
    assert(!op(&ctx, 0x40, 7, UNI_HAL_I2C_MEMADD_SIZE_8BIT, buffer, 1, 20));
    fail = false;
    compete = true;
    assert(op(&ctx, 0x40, 7, UNI_HAL_I2C_MEMADD_SIZE_8BIT, buffer, 1, 20));
    assert(!compete && accesses == initial_accesses + 1 && resets == initial_resets + 1);
    assert(irq_starts == initial_starts + 3 && !hi2c1.pBuffPtr);

    ctx.config.irq_enable = false;
    uint32_t prior_polls = polls;
    assert(op(&ctx, 0x40, 7, UNI_HAL_I2C_MEMADD_SIZE_8BIT, buffer, 1, 20));
    assert(polls == prior_polls + 1 && irq_starts == initial_starts + 3);
    ctx.config.irq_enable = true;

    deliver = false;
    complete_at_deadline = true;
    assert(op(&ctx, 0x40, 7, UNI_HAL_I2C_MEMADD_SIZE_8BIT, buffer, 1, 10));
    assert(!complete_at_deadline && resets == initial_resets + 1 && accesses == initial_accesses + 2);
    // A completion semaphore posted at the deadline cannot complete the next transfer.
    assert(!op(&ctx, 0x40, 7, UNI_HAL_I2C_MEMADD_SIZE_8BIT, buffer, 1, 10));
    assert(resets == initial_resets + 2 && accesses == initial_accesses + 2);
}

int main(void)
{
    assert(uni_hal_i2c_init(&ctx));
    exercise(uni_hal_i2c_mem_read);
    exercise(uni_hal_i2c_mem_write);
    assert(waits == 0 && delays == 0 && !active_event);
    scheduler_running = true;
    uint32_t starts_before_event = irq_starts;
#if !configSUPPORT_STATIC_ALLOCATION
    fail_event_allocation = true;
    assert(!uni_hal_i2c_mem_read(&ctx, 0x40, 7, UNI_HAL_I2C_MEMADD_SIZE_8BIT, buffer, 1, 20));
    assert(!active_event && irq_starts == starts_before_event);
    fail_event_allocation = false;
#endif
    // Semaphore creation can use the entire transfer deadline.
    event_creation_ms = 20;
    assert(!uni_hal_i2c_mem_read(&ctx, 0x40, 7, UNI_HAL_I2C_MEMADD_SIZE_8BIT, buffer, 1, 20));
    assert(active_event && irq_starts == starts_before_event);
    event_creation_ms = 0;
    in_irq = true;
    assert(!uni_hal_i2c_mem_read(&ctx, 0x40, 7, UNI_HAL_I2C_MEMADD_SIZE_8BIT, buffer, 1, 20));
    assert(!uni_hal_i2c_deinit(&ctx));
    in_irq = false;
    exercise(uni_hal_i2c_mem_read);
    exercise(uni_hal_i2c_mem_write);
    assert(delays > 0); // A competing task yields while another transfer owns the bus.
    assert(waits > 0);
    deliver = false;
    fail_init = true;
    assert(!uni_hal_i2c_mem_read(&ctx, 0x40, 7, UNI_HAL_I2C_MEMADD_SIZE_8BIT, buffer, 1, 10));
    assert(!ctx.state.initialized && !hi2c1.XferISR && !hi2c1.pBuffPtr);
    assert(uni_hal_i2c_deinit(&ctx));
    assert(!active_event);
    fail_init = false;
    deliver = true;
    assert(uni_hal_i2c_init(&ctx));
    assert(uni_hal_i2c_mem_read(&ctx, 0x40, 7, UNI_HAL_I2C_MEMADD_SIZE_8BIT, buffer, 1, 20));
    assert(active_event && uni_hal_i2c_deinit(&ctx) && !active_event);
    return 0;
}
