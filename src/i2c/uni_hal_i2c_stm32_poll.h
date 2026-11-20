#pragma once

#include "systick/uni_hal_systick.h"
#include <FreeRTOS.h>
#include <task.h>

// Called after claim_transfer accepts a 7-bit address or an already shifted address byte.
static inline uint16_t address_byte(uint16_t addr)
{
    return addr <= 0x7FU ? (uint16_t)(addr << 1U) : addr;
}

// The same millisecond budget covers the initial claim, ST polling and every HAL_BUSY retry.
static inline uint32_t remaining_ms(uint32_t start, uint32_t timeout)
{
    uint32_t elapsed = uni_hal_systick_get_ms() - start;
    return elapsed < timeout ? timeout - elapsed : 0U;
}

// ST interprets UINT32_MAX as an infinite wait. Uni.HAL timeouts are finite.
static inline uint32_t hal_timeout(uint32_t remaining)
{
    return remaining == UINT32_MAX ? UINT32_MAX - 1U : remaining;
}

// Blocking transfers are task/startup operations. At startup SysTick may still run
// above BASEPRI even while the FreeRTOS task-level interrupt mask is active.
static inline bool polling_allowed(void)
{
    uint32_t basepri = __get_BASEPRI();
    return !__get_IPSR() && !__get_PRIMASK() && !__get_FAULTMASK() &&
           (!basepri || (NVIC_GetPriority(SysTick_IRQn) << (8U - __NVIC_PRIO_BITS)) < basepri);
}

static inline uint32_t retry_budget(uint32_t start, uint32_t timeout)
{
    uint32_t remaining = remaining_ms(start, timeout);
    if (!__get_BASEPRI() && remaining > 1U && pdMS_TO_TICKS(remaining - 1U) != 0U &&
        xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        // Let a task holding the bus make progress. Reserve time for another attempt.
        vTaskDelay(1U);
        remaining = remaining_ms(start, timeout);
    }
    return remaining;
}

static inline bool finished_in_time(HAL_StatusTypeDef status, uint32_t start, uint32_t timeout)
{
    return status == HAL_OK && (uni_hal_systick_get_ms() - start) <= timeout;
}

static inline bool poll_ready(I2C_HandleTypeDef *handle, uint16_t addr, uint32_t trials,
                              uint32_t start, uint32_t timeout)
{
    for (uint32_t remaining = remaining_ms(start, timeout); remaining; remaining = retry_budget(start, timeout)) {
        HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(handle, addr, trials, hal_timeout(remaining));
        if (status != HAL_BUSY) {
            return finished_in_time(status, start, timeout);
        }
    }
    return false;
}

static inline bool poll_master(I2C_HandleTypeDef *handle, uint16_t addr, uint8_t *buf, uint16_t len,
                               bool read, uint32_t start, uint32_t timeout)
{
    for (uint32_t remaining = remaining_ms(start, timeout); remaining; remaining = retry_budget(start, timeout)) {
        uint32_t wait = hal_timeout(remaining);
        HAL_StatusTypeDef status = read ? HAL_I2C_Master_Receive(handle, addr, buf, len, wait)
                                        : HAL_I2C_Master_Transmit(handle, addr, buf, len, wait);
        if (status != HAL_BUSY) {
            return finished_in_time(status, start, timeout);
        }
    }
    return false;
}
