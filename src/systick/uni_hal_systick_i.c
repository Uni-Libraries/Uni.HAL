#include "uni_hal_target.h"
#include "systick/uni_hal_systick.h"

#if defined(UNI_HAL_TARGET_MCU_STM32L496) || defined(UNI_HAL_TARGET_STM32H7)

#include <cmsis_gcc.h>

#include "systick/uni_hal_systick_stm32.h"
#include <FreeRTOS.h>
#include <task.h>

extern void xPortSysTickHandler(void);
extern volatile uint32_t g_uni_hal_systick_counter;

// Numerator of the part of a millisecond left over from the last RTOS tick.
static uint32_t fraction;

void SysTick_Handler(void) {
    if (!uni_hal_systick_uses_rtos_period) {
        ++g_uni_hal_systick_counter;
        return;
    }

    uint32_t elapsed_ms = 1000U / configTICK_RATE_HZ;
    fraction += 1000U % configTICK_RATE_HZ;
    if (fraction >= configTICK_RATE_HZ) {
        fraction -= configTICK_RATE_HZ;
        ++elapsed_ms;
    }
    g_uni_hal_systick_counter += elapsed_ms;
    xPortSysTickHandler();
}

#if configUSE_TICKLESS_IDLE != 0
// The Cortex-M port calls vTaskStepTick with SysTick masked after a sleep. These
// suppressed periods had no IRQ and must advance the HAL clock as well.
extern void __real_vTaskStepTick(TickType_t ticks);
void __wrap_vTaskStepTick(TickType_t ticks) {
    __real_vTaskStepTick(ticks);
    // Custom sleep implementations may already have unmasked the periodic IRQ.
    uint32_t const primask = __get_PRIMASK();
    __disable_irq();
    uint64_t total = (uint64_t)ticks * 1000U + fraction;
    g_uni_hal_systick_counter += (uint32_t)(total / configTICK_RATE_HZ);
    fraction = (uint32_t)(total % configTICK_RATE_HZ);
    __set_PRIMASK(primask);
}
#endif

#endif
