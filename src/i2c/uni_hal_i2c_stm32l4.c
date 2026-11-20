//
// Includes
//

// stdlib
#include <stddef.h>

// STM
#include <stm32l4xx_hal.h>
#include <stm32l4xx_ll_i2c.h>

// FreeRTOS
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

// Uni.Common
#include <uni_common.h>

// Uni.HAL
#include "os/uni_hal_os.h"
#include "i2c/uni_hal_i2c.h"
#include "core/uni_hal_core.h"
#include "gpio/uni_hal_gpio.h"
#include "rcc/uni_hal_rcc.h"
#include "uni_hal_i2c_stm32_poll.h"



//
// Globals
//

UNI_COMMON_COMPILER_WEAK I2C_HandleTypeDef hi2c1 = {0};
UNI_COMMON_COMPILER_WEAK I2C_HandleTypeDef hi2c2 = {0};
UNI_COMMON_COMPILER_WEAK I2C_HandleTypeDef hi2c3 = {0};
UNI_COMMON_COMPILER_WEAK I2C_HandleTypeDef hi2c4 = {0};

typedef enum {
    IRQ_IDLE,
    IRQ_PENDING,
    IRQ_SUCCEEDED,
    IRQ_FAILED,
} i2c_irq_state_t;

typedef struct {
    uni_hal_i2c_context_t *owner;
    bool busy;
    volatile i2c_irq_state_t irq_state;
    uint32_t completed_at_ms;
    bool wait_with_event;
    BaseType_t higher_priority_woken;
    SemaphoreHandle_t event;
#if configSUPPORT_STATIC_ALLOCATION
    StaticSemaphore_t event_storage;
#endif
} i2c_bus_t;

static i2c_bus_t g_buses[UNI_HAL_I2C_MAXINSTANCES];


//
// IRQ
//

static void handle_irq(I2C_HandleTypeDef *handle, unsigned index, bool error)
{
    // Event and error IRQs for this bus have the same priority. Other buses may preempt them.
    g_buses[index].higher_priority_woken = pdFALSE;
    UNI_HAL_OS_ISR_ENTER();
    if (error) {
        HAL_I2C_ER_IRQHandler(handle);
    } else {
        HAL_I2C_EV_IRQHandler(handle);
    }
    UNI_HAL_OS_ISR_EXIT(g_buses[index].higher_priority_woken);
}

void I2C1_EV_IRQHandler(void) { handle_irq(&hi2c1, 0, false); }
void I2C2_EV_IRQHandler(void) { handle_irq(&hi2c2, 1, false); }
void I2C3_EV_IRQHandler(void) { handle_irq(&hi2c3, 2, false); }
void I2C4_EV_IRQHandler(void) { handle_irq(&hi2c4, 3, false); }

void I2C1_ER_IRQHandler(void) { handle_irq(&hi2c1, 0, true); }
void I2C2_ER_IRQHandler(void) { handle_irq(&hi2c2, 1, true); }
void I2C3_ER_IRQHandler(void) { handle_irq(&hi2c3, 2, true); }
void I2C4_ER_IRQHandler(void) { handle_irq(&hi2c4, 3, true); }



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


static uni_hal_core_periph_e _uni_hal_i2c_get_periph(I2C_TypeDef* instance) {
    uni_hal_core_periph_e result = UNI_HAL_CORE_PERIPH_UNKNOWN;

    if (instance == I2C1)
    {
        result = UNI_HAL_CORE_PERIPH_I2C_1;
    }
    else if (instance == I2C2)
    {
        result = UNI_HAL_CORE_PERIPH_I2C_2;
    }
    else if (instance == I2C3)
    {
        result = UNI_HAL_CORE_PERIPH_I2C_3;
    }
    else if (instance == I2C4)
    {
        result = UNI_HAL_CORE_PERIPH_I2C_4;
    }

    return result;
}

static uint32_t _uni_hal_i2c_get_number(uni_hal_core_periph_e instance) {
    uint32_t result = UINT32_MAX;

    switch (instance) {
    case UNI_HAL_CORE_PERIPH_I2C_1:
        result = 1;
        break;
    case UNI_HAL_CORE_PERIPH_I2C_2:
        result = 2;
        break;
    case UNI_HAL_CORE_PERIPH_I2C_3:
        result = 3;
        break;
    case UNI_HAL_CORE_PERIPH_I2C_4:
        result = 4;
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


static bool _uni_hal_i2c_init_irq(uni_hal_i2c_context_t *ctx)
{
    bool result = false;

    uni_hal_core_irq_e irq_1 = UNI_HAL_CORE_IRQ_UNKNOWN;
    uni_hal_core_irq_e irq_2 = UNI_HAL_CORE_IRQ_UNKNOWN;

    switch (ctx->config.instance)
    {
        case UNI_HAL_CORE_PERIPH_I2C_1:
            irq_1 = UNI_HAL_CORE_IRQ_I2C_1_IRQ1;
            irq_2 = UNI_HAL_CORE_IRQ_I2C_1_IRQ2;
            break;
        case UNI_HAL_CORE_PERIPH_I2C_2:
            irq_1 = UNI_HAL_CORE_IRQ_I2C_2_IRQ1;
            irq_2 = UNI_HAL_CORE_IRQ_I2C_2_IRQ2;
            break;
        case UNI_HAL_CORE_PERIPH_I2C_3:
            irq_1 = UNI_HAL_CORE_IRQ_I2C_3_IRQ1;
            irq_2 = UNI_HAL_CORE_IRQ_I2C_3_IRQ2;
            break;
        case UNI_HAL_CORE_PERIPH_I2C_4:
            irq_1 = UNI_HAL_CORE_IRQ_I2C_4_IRQ1;
            irq_2 = UNI_HAL_CORE_IRQ_I2C_4_IRQ2;
            break;
        default:
            break;
    }

    if (irq_1 != UNI_HAL_CORE_IRQ_UNKNOWN || irq_2 != UNI_HAL_CORE_IRQ_UNKNOWN)
    {
        result = true;
        if (irq_1 != UNI_HAL_CORE_IRQ_UNKNOWN)
        {
            result = result && uni_hal_core_irq_enable(irq_1, ctx->config.irq_priority, 0U);
        }
        if (irq_2 != UNI_HAL_CORE_IRQ_UNKNOWN)
        {
            result = result && uni_hal_core_irq_enable(irq_2, ctx->config.irq_priority, 0U);
        }
    }

    return result;
}


static i2c_bus_t *get_bus(uni_hal_core_periph_e instance)
{
    uint32_t number = _uni_hal_i2c_get_number(instance);
    return number <= UNI_HAL_I2C_MAXINSTANCES ? &g_buses[number - 1U] : NULL;
}

static bool try_claim(i2c_bus_t *bus)
{
    uint32_t mask = uni_hal_core_irq_pause();
    bool claimed = !bus->busy;
    if (claimed) {
        bus->busy = true;
    }
    uni_hal_core_irq_resume(mask);
    return claimed;
}

static void release_bus(i2c_bus_t *bus)
{
    uint32_t mask = uni_hal_core_irq_pause();
    bus->busy = false;
    uni_hal_core_irq_resume(mask);
}

static i2c_bus_t *claim_transfer(uni_hal_i2c_context_t *ctx, uint32_t start, uint32_t timeout)
{
    if (!timeout || !polling_allowed() || !uni_hal_i2c_is_inited(ctx)) {
        return NULL;
    }
    i2c_bus_t *bus = get_bus(ctx->config.instance);
    if (!bus) {
        return NULL;
    }
    do {
        if (try_claim(bus)) {
            if (bus->owner == ctx && ctx->state.initialized) {
                return bus;
            }
            release_bus(bus);
            return NULL;
        }
    } while (retry_budget(start, timeout));
    return NULL;
}

static void _uni_hal_i2c_callback(I2C_HandleTypeDef *handle, bool success)
{
    i2c_bus_t *bus = get_bus(_uni_hal_i2c_get_periph(handle->Instance));
    if (bus && bus->irq_state == IRQ_PENDING) {
        bus->completed_at_ms = uni_hal_systick_get_ms();
        bus->irq_state = success ? IRQ_SUCCEEDED : IRQ_FAILED;
        if (bus->wait_with_event) {
            xSemaphoreGiveFromISR(bus->event, &bus->higher_priority_woken);
        }
    }
}



//
// Callbacks
//

void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    _uni_hal_i2c_callback(hi2c, true);
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    _uni_hal_i2c_callback(hi2c, true);
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    _uni_hal_i2c_callback(hi2c, false);
}

void HAL_I2C_AbortCpltCallback(I2C_HandleTypeDef *hi2c)
{
    _uni_hal_i2c_callback(hi2c, false);
}



//
// Public
//

bool uni_hal_i2c_init(uni_hal_i2c_context_t *ctx) {
    bool result = false;

    if (ctx != NULL && !uni_hal_i2c_is_inited(ctx))
    {
        I2C_TypeDef *handle = _uni_hal_i2c_get_handle(ctx->config.instance);
        I2C_HandleTypeDef *handle_hal = _uni_hal_i2c_get_handle_hal(ctx->config.instance);
        i2c_bus_t *bus = get_bus(ctx->config.instance);
        if (handle && handle_hal && try_claim(bus)) {
            if (bus->owner && bus->owner != ctx) {
                release_bus(bus);
                return false;
            }
            result = uni_hal_rcc_clk_set(ctx->config.instance, true);

            if (!uni_hal_gpio_pin_is_inited(ctx->config.pin_sck)) {
                ctx->config.pin_sck->gpio_type = UNI_HAL_GPIO_TYPE_ALTERNATE_OD,
                result = uni_hal_gpio_pin_init(ctx->config.pin_sck) && result;
            }
            if(!uni_hal_gpio_pin_is_inited(ctx->config.pin_sda)) {
                ctx->config.pin_sda->gpio_type = UNI_HAL_GPIO_TYPE_ALTERNATE_OD,
                result = uni_hal_gpio_pin_init(ctx->config.pin_sda) && result;
            }

            if(result) {
                handle_hal->Instance = handle;

                // the timing depends on the clock that feeds the peripheral, so it is computed from it
                handle_hal->Init.Timing = ctx->config.timing != 0U
                        ? ctx->config.timing
                        : uni_hal_i2c_timing_calc(uni_hal_rcc_clk_get_freq(ctx->config.instance), ctx->config.speed);
                result = handle_hal->Init.Timing != 0U;

                handle_hal->Init.OwnAddress1 = 0;
                handle_hal->Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
                handle_hal->Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
                handle_hal->Init.OwnAddress2 = 0;
                handle_hal->Init.OwnAddress2Masks = I2C_OA2_NOMASK;
                handle_hal->Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
                handle_hal->Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
                result = result && (HAL_I2C_Init(handle_hal) == HAL_OK);

                if(result) {
                    result = HAL_I2CEx_ConfigAnalogFilter(handle_hal, I2C_ANALOGFILTER_ENABLE) == HAL_OK;
                }

                if(result){
                    result = HAL_I2CEx_ConfigDigitalFilter(handle_hal, 0) == HAL_OK;
                }
            }

            if (result && ctx->config.irq_enable)
            {
                result = _uni_hal_i2c_init_irq(ctx);
            }

            if (result)
            {
                bus->owner = ctx;
            }

            ctx->state.initialized = result;
            release_bus(bus);
        }
    }

    return result;
}


bool uni_hal_i2c_deinit(uni_hal_i2c_context_t *ctx) {
    i2c_bus_t *bus = ctx ? get_bus(ctx->config.instance) : NULL;
    if (__get_IPSR() || !bus || !try_claim(bus)) {
        return false;
    }
    if (bus->owner && bus->owner != ctx) {
        release_bus(bus);
        return false;
    }
    I2C_HandleTypeDef *handle = _uni_hal_i2c_get_handle_hal(ctx->config.instance);
    if (handle->Instance) {
        HAL_I2C_DeInit(handle);
        LL_I2C_DeInit(handle->Instance);
    }
    if (bus->event) {
        vSemaphoreDelete(bus->event);
        bus->event = NULL;
    }
    bus->owner = NULL;
    ctx->state.initialized = false;
    release_bus(bus);
    return true;
}


bool uni_hal_i2c_isready(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint32_t trials, uint32_t timeout) {
    bool result = false;

    uint32_t const start_ms = uni_hal_systick_get_ms();
    i2c_bus_t *bus = claim_transfer(ctx, start_ms, timeout);
    if (bus) {
        result = poll_ready(_uni_hal_i2c_get_handle_hal(ctx->config.instance), _uni_hal_i2c_shiftaddr(dev_addr),
                            trials, start_ms, timeout);
        release_bus(bus);
    }

    return result;
}


bool uni_hal_i2c_master_receive(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint8_t *buf, uint16_t buf_len, uint32_t timeout) {
    bool result = false;

    uint32_t const start_ms = uni_hal_systick_get_ms();
    i2c_bus_t *bus = claim_transfer(ctx, start_ms, timeout);
    if (bus) {
        result = poll_master(_uni_hal_i2c_get_handle_hal(ctx->config.instance), _uni_hal_i2c_shiftaddr(dev_addr),
                             buf, buf_len, true, start_ms, timeout);
        release_bus(bus);
    }

    return result;
}


bool uni_hal_i2c_master_transmit(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint8_t *buf, uint16_t buf_len, uint32_t timeout) {
    bool result = false;

    uint32_t const start_ms = uni_hal_systick_get_ms();
    i2c_bus_t *bus = claim_transfer(ctx, start_ms, timeout);
    if (bus) {
        result = poll_master(_uni_hal_i2c_get_handle_hal(ctx->config.instance), _uni_hal_i2c_shiftaddr(dev_addr),
                             buf, buf_len, false, start_ms, timeout);
        release_bus(bus);
    }

    return result;
}


// The owner holds the bus throughout setup, the IRQ wait and synchronous cancellation.
static bool mem_transfer(bool read, uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint16_t mem_addr,
                         uni_hal_i2c_memadd_size_e mem_addr_size, uint8_t *buf, uint16_t len, uint32_t timeout)
{
    uint32_t const start = uni_hal_systick_get_ms();
    i2c_bus_t *bus = claim_transfer(ctx, start, timeout);
    if (!bus) {
        return false;
    }
    I2C_HandleTypeDef *handle = _uni_hal_i2c_get_handle_hal(ctx->config.instance);
    uint16_t addr = _uni_hal_i2c_shiftaddr(dev_addr);
    uint16_t size = _uni_hal_i2c_memadd_size(mem_addr_size);
    bool result = false;
    uint32_t remaining = remaining_ms(start, timeout);
    if (!remaining || HAL_I2C_GetState(handle) != HAL_I2C_STATE_READY || !buf || !len || !size) {
        release_bus(bus);
        return false;
    }
    if (!ctx->config.irq_enable) {
        HAL_StatusTypeDef status = read ? HAL_I2C_Mem_Read(handle, addr, mem_addr, size, buf, len, hal_timeout(remaining))
                                        : HAL_I2C_Mem_Write(handle, addr, mem_addr, size, buf, len, hal_timeout(remaining));
        release_bus(bus);
        return finished_in_time(status, start, timeout);
    }

    bool scheduler_running = xTaskGetSchedulerState() == taskSCHEDULER_RUNNING;
    if (scheduler_running) {
        if (!bus->event) {
#if configSUPPORT_STATIC_ALLOCATION
            bus->event = xSemaphoreCreateBinaryStatic(&bus->event_storage);
#else
            bus->event = xSemaphoreCreateBinary();
#endif
            if (!bus->event) {
                release_bus(bus);
                return false;
            }
        }
        // Completion can race the timeout or arrive before the first wait.
        (void)xSemaphoreTake(bus->event, 0U);
    }
    uint32_t mask = uni_hal_core_irq_pause();
    if (!remaining_ms(start, timeout)) {
        uni_hal_core_irq_resume(mask);
        release_bus(bus);
        return false;
    }
    bus->wait_with_event = scheduler_running;
    bus->irq_state = IRQ_PENDING;
    HAL_StatusTypeDef status = read ? HAL_I2C_Mem_Read_IT(handle, addr, mem_addr, size, buf, len)
                                    : HAL_I2C_Mem_Write_IT(handle, addr, mem_addr, size, buf, len);
    if (status != HAL_OK) {
        bus->irq_state = IRQ_IDLE;
    }
    uni_hal_core_irq_resume(mask);

    while (bus->irq_state == IRQ_PENDING && (remaining = remaining_ms(start, timeout)) != 0U) {
        if (scheduler_running) {
            // portMAX_DELAY can mean forever. Recheck the same deadline after each wait.
            TickType_t ticks = pdMS_TO_TICKS(remaining);
            if (ticks == portMAX_DELAY) {
                --ticks;
            }
            (void)xSemaphoreTake(bus->event, ticks ? ticks : 1U);
        }
    }

    mask = uni_hal_core_irq_pause();
    if (bus->irq_state == IRQ_PENDING) {
        // No STOP/abort interrupt is guaranteed on a stalled bus. Reset the peripheral
        // synchronously, including its interrupt sources, before releasing caller storage.
        handle->XferISR = NULL;
        handle->pBuffPtr = NULL;
        handle->XferCount = 0U;
        handle->XferSize = 0U;
        bool reset = LL_I2C_DeInit(handle->Instance) == SUCCESS;
        // HAL init reapplies TIMINGR and the driver's analog-on/digital-off filter defaults.
        ctx->state.initialized = reset && HAL_I2C_Init(handle) == HAL_OK;
    } else if (status == HAL_OK) {
        // The waiter may resume late even when the interrupt completed on time.
        result = bus->irq_state == IRQ_SUCCEEDED && bus->completed_at_ms - start <= timeout;
        handle->pBuffPtr = NULL;
    }
    bus->irq_state = IRQ_IDLE;
    bus->wait_with_event = false;
    uni_hal_core_irq_resume(mask);
    release_bus(bus);
    return result;
}

bool uni_hal_i2c_mem_read(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint16_t mem_addr,
                          uni_hal_i2c_memadd_size_e mem_addr_size, uint8_t *buf, uint16_t buf_len, uint32_t timeout)
{
    return mem_transfer(true, ctx, dev_addr, mem_addr, mem_addr_size, buf, buf_len, timeout);
}

bool uni_hal_i2c_mem_write(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint16_t mem_addr,
                           uni_hal_i2c_memadd_size_e mem_addr_size, uint8_t *buf, uint16_t buf_len, uint32_t timeout)
{
    return mem_transfer(false, ctx, dev_addr, mem_addr, mem_addr_size, buf, buf_len, timeout);
}
