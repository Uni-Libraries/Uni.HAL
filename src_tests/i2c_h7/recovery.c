#undef NDEBUG
#include <assert.h>
#include <stddef.h>

#include "i2c/uni_hal_i2c.h"
#include "stm32h7xx_hal.h"
#include "stm32h7xx_ll_i2c.h"

I2C_TypeDef model_instances[4];
uint32_t __get_IPSR(void) { return 0U; }
uint32_t __get_PRIMASK(void) { return 0U; }
uint32_t __get_BASEPRI(void) { return 0U; }
uint32_t NVIC_GetPriority(int irq) { (void)irq; return 0U; }

static unsigned hal_inits, hal_deinits, peripheral_resets;
static bool fail_deinit, fail_reset, fail_filter;

HAL_StatusTypeDef HAL_I2C_Init(I2C_HandleTypeDef *handle) {
    assert(handle->Instance == I2C1 && handle->Init.Timing == 42U);
    ++hal_inits;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_I2C_DeInit(I2C_HandleTypeDef *handle) {
    assert(handle->Instance == I2C1);
    ++hal_deinits;
    return fail_deinit ? HAL_ERROR : HAL_OK;
}

ErrorStatus LL_I2C_DeInit(const I2C_TypeDef *instance) {
    assert(instance == I2C1);
    ++peripheral_resets;
    return fail_reset ? ERROR : SUCCESS;
}

HAL_StatusTypeDef HAL_I2CEx_ConfigAnalogFilter(I2C_HandleTypeDef *handle, uint32_t value) {
    assert(handle->Instance == I2C1 && value == I2C_ANALOGFILTER_ENABLE);
    return fail_filter ? HAL_ERROR : HAL_OK;
}

HAL_StatusTypeDef HAL_I2CEx_ConfigDigitalFilter(I2C_HandleTypeDef *handle, uint32_t value) {
    assert(handle->Instance == I2C1 && value == 0U);
    return HAL_OK;
}

// This fixture only exercises lifecycle calls. The transfer entry points must remain unused.
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *, uint16_t, uint32_t, uint32_t) {
    assert(false);
    return HAL_ERROR;
}

HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef *, uint16_t, uint8_t *, uint16_t, uint32_t) {
    assert(false);
    return HAL_ERROR;
}

HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *, uint16_t, uint8_t *, uint16_t, uint32_t) {
    assert(false);
    return HAL_ERROR;
}

HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *, uint16_t, uint16_t, uint16_t, uint8_t *, uint16_t, uint32_t) {
    assert(false);
    return HAL_ERROR;
}

HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *, uint16_t, uint16_t, uint16_t, uint8_t *, uint16_t, uint32_t) {
    assert(false);
    return HAL_ERROR;
}

int main(void) {
    uni_hal_gpio_pin_context_t scl = {.gpio_type = UNI_HAL_GPIO_TYPE_ALTERNATE_OD};
    uni_hal_gpio_pin_context_t sda = {.gpio_type = UNI_HAL_GPIO_TYPE_ALTERNATE_OD};
    uni_hal_i2c_context_t ctx = {.config = {
        .instance = UNI_HAL_CORE_PERIPH_I2C_1, .pin_sck = &scl, .pin_sda = &sda, .timing = 42U,
    }};
    uni_hal_i2c_context_t other = ctx;
    uni_hal_i2c_context_t invalid = {0};

    assert(!uni_hal_i2c_deinit(nullptr));
    assert(!uni_hal_i2c_deinit(&invalid));
    assert(uni_hal_i2c_recover(&ctx));
    assert(hal_deinits == 0U && peripheral_resets == 1U && hal_inits == 1U);
    assert(ctx.state.initialized && scl.inited && sda.inited);
    assert(scl.gpio_type == UNI_HAL_GPIO_TYPE_ALTERNATE_OD && sda.gpio_type == UNI_HAL_GPIO_TYPE_ALTERNATE_OD);

    assert(!uni_hal_i2c_recover(&other));
    assert(!uni_hal_i2c_init(&other));
    assert(ctx.state.initialized && peripheral_resets == 1U);
    assert(uni_hal_i2c_reset(&ctx));
    assert(hal_deinits == 1U && peripheral_resets == 2U && hal_inits == 2U);

    fail_deinit = true;
    assert(!uni_hal_i2c_recover(&ctx));
    assert(!ctx.state.initialized && hal_inits == 2U && peripheral_resets == 2U);
    fail_deinit = false;
    fail_reset = true;
    assert(!uni_hal_i2c_recover(&ctx));
    assert(hal_deinits == 3U && peripheral_resets == 3U && hal_inits == 2U);
    fail_reset = false;
    assert(uni_hal_i2c_recover(&ctx));
    assert(hal_deinits == 3U && peripheral_resets == 4U && hal_inits == 3U);

    assert(uni_hal_i2c_deinit(&ctx));
    assert(uni_hal_i2c_deinit(&ctx));
    assert(!ctx.state.initialized && hal_deinits == 4U);
    fail_filter = true;
    assert(!uni_hal_i2c_init(&other));
    assert(!other.state.initialized && hal_deinits == 5U);
    fail_filter = false;
    assert(uni_hal_i2c_recover(&other));
    assert(uni_hal_i2c_deinit(&other));
    return 0;
}
