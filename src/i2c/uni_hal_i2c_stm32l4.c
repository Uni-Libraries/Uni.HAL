//
// Includes
//

// stdlib
#include <stddef.h>

// STM
#include <stm32l496xx.h>
#include <stm32l4xx_hal.h>
#include <stm32l4xx_hal_i2c.h>
#include <stm32l4xx_ll_bus.h>
#include <stm32l4xx_ll_i2c.h>
#include <stm32l4xx_ll_rcc.h>

// FreeRTOS
#if defined(UNI_HAL_I2C_USE_FREERTOS)
    #include <FreeRTOS.h>
    #include <task.h>
#endif

// Uni.Common
#include <uni_common.h>

// Uni.HAL
#include "i2c/uni_hal_i2c.h"
#include "core/uni_hal_core.h"
#include "gpio/uni_hal_gpio.h"
#include "rcc/uni_hal_rcc.h"



//
// Globals
//

UNI_COMMON_COMPILER_WEAK I2C_HandleTypeDef hi2c1 = {0};
UNI_COMMON_COMPILER_WEAK I2C_HandleTypeDef hi2c2 = {0};
UNI_COMMON_COMPILER_WEAK I2C_HandleTypeDef hi2c3 = {0};
UNI_COMMON_COMPILER_WEAK I2C_HandleTypeDef hi2c4 = {0};

static uni_hal_i2c_context_t *g_uni_hal_i2c_ctx[UNI_HAL_I2C_MAXINSTANCES] = {nullptr};

#if defined(UNI_HAL_I2C_USE_FREERTOS)
    static BaseType_t g_uni_hal_i2c_highprio_woken = pdFALSE;
#endif



//
// IRQ
//

void I2C1_EV_IRQHandler(void)
{
#if defined(UNI_HAL_I2C_USE_FREERTOS)
    traceISR_ENTER();
    g_uni_hal_i2c_highprio_woken = pdFALSE;
#endif

    HAL_I2C_EV_IRQHandler(&hi2c1);

#if defined(UNI_HAL_I2C_USE_FREERTOS)
    portYIELD_FROM_ISR( g_uni_hal_i2c_highprio_woken );
#endif
}

void I2C2_EV_IRQHandler(void)
{
#if defined(UNI_HAL_I2C_USE_FREERTOS)
    traceISR_ENTER();
    g_uni_hal_i2c_highprio_woken = pdFALSE;
#endif

    HAL_I2C_EV_IRQHandler(&hi2c2);

#if defined(UNI_HAL_I2C_USE_FREERTOS)
    portYIELD_FROM_ISR( g_uni_hal_i2c_highprio_woken );
#endif
}

void I2C3_EV_IRQHandler(void)
{
#if defined(UNI_HAL_I2C_USE_FREERTOS)
    traceISR_ENTER();
    g_uni_hal_i2c_highprio_woken = pdFALSE;
#endif

    HAL_I2C_EV_IRQHandler(&hi2c4);

#if defined(UNI_HAL_I2C_USE_FREERTOS)
    portYIELD_FROM_ISR( g_uni_hal_i2c_highprio_woken );
#endif
}

void I2C4_EV_IRQHandler(void)
{
#if defined(UNI_HAL_I2C_USE_FREERTOS)
    traceISR_ENTER();
    g_uni_hal_i2c_highprio_woken = pdFALSE;
#endif

    HAL_I2C_EV_IRQHandler(&hi2c4);

#if defined(UNI_HAL_I2C_USE_FREERTOS)
    portYIELD_FROM_ISR( g_uni_hal_i2c_highprio_woken );
#endif
}

void I2C1_ER_IRQHandler(void)
{
#if defined(UNI_HAL_I2C_USE_FREERTOS)
    traceISR_ENTER();
    g_uni_hal_i2c_highprio_woken = pdFALSE;
#endif

    HAL_I2C_ER_IRQHandler(&hi2c1);

#if defined(UNI_HAL_I2C_USE_FREERTOS)
    portYIELD_FROM_ISR( g_uni_hal_i2c_highprio_woken );
#endif
}

void I2C2_ER_IRQHandler(void)
{
#if defined(UNI_HAL_I2C_USE_FREERTOS)
    traceISR_ENTER();
    g_uni_hal_i2c_highprio_woken = pdFALSE;
#endif

    HAL_I2C_ER_IRQHandler(&hi2c2);

#if defined(UNI_HAL_I2C_USE_FREERTOS)
    portYIELD_FROM_ISR( g_uni_hal_i2c_highprio_woken );
#endif
}

void I2C3_ER_IRQHandler(void)
{
#if defined(UNI_HAL_I2C_USE_FREERTOS)
    traceISR_ENTER();
    g_uni_hal_i2c_highprio_woken = pdFALSE;
#endif

    HAL_I2C_ER_IRQHandler(&hi2c4);

#if defined(UNI_HAL_I2C_USE_FREERTOS)
    portYIELD_FROM_ISR( g_uni_hal_i2c_highprio_woken );
#endif
}

void I2C4_ER_IRQHandler(void)
{
#if defined(UNI_HAL_I2C_USE_FREERTOS)
    traceISR_ENTER();
    g_uni_hal_i2c_highprio_woken = pdFALSE;
#endif

    HAL_I2C_ER_IRQHandler(&hi2c4);

#if defined(UNI_HAL_I2C_USE_FREERTOS)
    portYIELD_FROM_ISR( g_uni_hal_i2c_highprio_woken );
#endif
}



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


static void _uni_hal_i2c_callback(I2C_HandleTypeDef* hal_handle, bool success)
{
    uni_hal_i2c_context_t* ctx = g_uni_hal_i2c_ctx[_uni_hal_i2c_get_number(_uni_hal_i2c_get_periph(hal_handle->Instance))-1];
    if (ctx)
    {
#if defined(UNI_HAL_I2C_USE_FREERTOS)
        xTaskNotifyIndexedFromISR(ctx->state.irq_task, 0, success ? 1U : 0U, eSetBits, &g_uni_hal_i2c_highprio_woken);
#endif
    }
}



//
// Callbacks
//

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    _uni_hal_i2c_callback(hi2c, true);
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    _uni_hal_i2c_callback(hi2c, true);
}

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
        if (handle != NULL && handle_hal != NULL &&  !ctx->state.initialized) {
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

                switch(ctx->config.speed) {
                case UNI_HAL_I2C_SPEED_400KHZ:
                    handle_hal->Init.Timing = 0x0090194F;
                    break;
                case UNI_HAL_I2C_SPEED_1MHZ:
                    handle_hal->Init.Timing = 0x00700815;
                    break;
                case UNI_HAL_I2C_SPEED_100KHZ:
                default:
                    handle_hal->Init.Timing = 0xC0100F14;
                    break;
                }

                handle_hal->Init.OwnAddress1 = 0;
                handle_hal->Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
                handle_hal->Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
                handle_hal->Init.OwnAddress2 = 0;
                handle_hal->Init.OwnAddress2Masks = I2C_OA2_NOMASK;
                handle_hal->Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
                handle_hal->Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
                result = HAL_I2C_Init(handle_hal) == HAL_OK;

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
                g_uni_hal_i2c_ctx[_uni_hal_i2c_get_number(ctx->config.instance)-1] = ctx;
            }

            ctx->state.initialized = result;
        }
    }

    return result;
}


bool uni_hal_i2c_deinit(uni_hal_i2c_context_t *ctx) {
    bool result = false;
    if (ctx != NULL) {
        I2C_HandleTypeDef *handle_hal = _uni_hal_i2c_get_handle_hal(ctx->config.instance);
        if (handle_hal != NULL) {
            HAL_I2C_DeInit(handle_hal);
            LL_I2C_Disable(handle_hal->Instance);
            LL_I2C_DeInit(handle_hal->Instance);
        }
        ctx->state.initialized = false;
        result = true;
    }
    return result;
}


bool uni_hal_i2c_isready(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint32_t trials, uint32_t timeout) {
    bool result = false;

    if (uni_hal_i2c_is_inited(ctx)) {
        HAL_StatusTypeDef status = HAL_BUSY;
        while (status == HAL_BUSY) {
            status = HAL_I2C_IsDeviceReady(_uni_hal_i2c_get_handle_hal(ctx->config.instance), _uni_hal_i2c_shiftaddr(dev_addr), trials, timeout);
        }

        result = status == HAL_OK;
    }

    return result;
}


bool uni_hal_i2c_master_receive(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint8_t *buf, uint16_t buf_len, uint32_t timeout) {
    bool result = false;

    if (uni_hal_i2c_is_inited(ctx)) {
        HAL_StatusTypeDef status = HAL_BUSY;
        while (status == HAL_BUSY) {
            status =
                HAL_I2C_Master_Receive(_uni_hal_i2c_get_handle_hal(ctx->config.instance), _uni_hal_i2c_shiftaddr(dev_addr), buf, buf_len, timeout);
        }

        result = status == HAL_OK;
    }

    return result;
}


bool uni_hal_i2c_master_transmit(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint8_t *buf, uint16_t buf_len, uint32_t timeout) {
    bool result = false;

    if (uni_hal_i2c_is_inited(ctx)) {
        HAL_StatusTypeDef status = HAL_BUSY;
        while (status == HAL_BUSY) {
            status =
                HAL_I2C_Master_Transmit(_uni_hal_i2c_get_handle_hal(ctx->config.instance), _uni_hal_i2c_shiftaddr(dev_addr), buf, buf_len, timeout);
        }

        result = status == HAL_OK;
    }

    return result;
}


bool uni_hal_i2c_mem_read(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint16_t mem_addr, uni_hal_i2c_memadd_size_e mem_addr_size,
                         uint8_t *buf, uint16_t buf_len, uint32_t timeout) {
    bool result = false;

    if (uni_hal_i2c_is_inited(ctx)) {
        I2C_HandleTypeDef* handle = _uni_hal_i2c_get_handle_hal(ctx->config.instance);
        if (handle)
        {
            while (HAL_I2C_GetState(handle) != HAL_I2C_STATE_READY)
            {
#if defined(UNI_HAL_I2C_USE_FREERTOS)
                taskYIELD();
#endif
            }

            if (ctx->config.irq_enable)
            {
#if defined(UNI_HAL_I2C_USE_FREERTOS)
                ctx->state.irq_task = xTaskGetCurrentTaskHandle();
#endif

                result = HAL_I2C_Mem_Read_IT(handle, _uni_hal_i2c_shiftaddr(dev_addr), mem_addr,
                    _uni_hal_i2c_memadd_size((mem_addr_size)), buf,buf_len) == HAL_OK;

#if defined(UNI_HAL_I2C_USE_FREERTOS)
                if (result && timeout)
                {
                    uint32_t notified_val = 0U;
                    result = xTaskNotifyWaitIndexed(0U, 0U, UINT32_MAX, &notified_val, pdMS_TO_TICKS(timeout)) == pdTRUE;
                    if (result)
                    {
                        result = notified_val == 1U;
                    }
                }
#endif
            }
            else
            {
                result = HAL_I2C_Mem_Read(handle, _uni_hal_i2c_shiftaddr(dev_addr),mem_addr,
                    _uni_hal_i2c_memadd_size((mem_addr_size)), buf, buf_len, timeout) == HAL_OK;
            }
        }
    }

    return result;
}


bool uni_hal_i2c_mem_write(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint16_t mem_addr, uni_hal_i2c_memadd_size_e mem_addr_size,
                          uint8_t *buf, uint16_t buf_len, uint32_t timeout) {
    bool result = false;

    if (uni_hal_i2c_is_inited(ctx)) {
        I2C_HandleTypeDef* handle = _uni_hal_i2c_get_handle_hal(ctx->config.instance);
        if (handle)
        {
            while (HAL_I2C_GetState(handle) != HAL_I2C_STATE_READY)
            {
#if defined(UNI_HAL_I2C_USE_FREERTOS)
                taskYIELD();
#endif
            }

            if (ctx->config.irq_enable)
            {
#if defined(UNI_HAL_I2C_USE_FREERTOS)
                ctx->state.irq_task = xTaskGetCurrentTaskHandle();
#endif

                result = HAL_I2C_Mem_Write_IT(handle, _uni_hal_i2c_shiftaddr(dev_addr), mem_addr,
                    _uni_hal_i2c_memadd_size((mem_addr_size)), buf,buf_len) == HAL_OK;

#if defined(UNI_HAL_I2C_USE_FREERTOS)
                if (result && timeout)
                {
                    uint32_t notified_val = 0U;
                    result = xTaskNotifyWaitIndexed(0U, 0U, UINT32_MAX, &notified_val, pdMS_TO_TICKS(timeout)) == pdTRUE;
                    if (result)
                    {
                        result = notified_val == 1U;
                    }
                }
#endif
            }
            else
            {
                result = HAL_I2C_Mem_Write(handle, _uni_hal_i2c_shiftaddr(dev_addr),mem_addr,
                    _uni_hal_i2c_memadd_size((mem_addr_size)), buf, buf_len, timeout) == HAL_OK;
            }
        }
    }

    return result;
}
