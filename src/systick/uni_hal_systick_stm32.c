#if defined(UNI_HAL_TARGET_MCU_STM32L496)
#include <stm32l4xx.h>
#else
#include <stm32h7xx.h>
#endif

#include <FreeRTOS.h>
#include <task.h>

#include "systick/uni_hal_systick.h"
#include "systick/uni_hal_systick_stm32.h"

extern void vPortSetupTimerInterrupt(void);
extern BaseType_t __real_xPortStartScheduler(void);

volatile bool uni_hal_systick_uses_rtos_period;

BaseType_t __wrap_xPortStartScheduler(void) {
    // FreeRTOS has marked the scheduler running, but the 1 kHz boot IRQ can still
    // preempt its mask. Stop that timer and discard a pending boot exception before
    // the port installs its own rate and lowers the interrupt priority.
    SysTick->CTRL = 0U;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
    __DSB();
    __ISB();
    uni_hal_systick_uses_rtos_period = true;
    return __real_xPortStartScheduler();
}

bool uni_hal_systick_init(void) {
    SystemCoreClockUpdate();
    if (uni_hal_systick_uses_rtos_period) {
        // The port owns the reload, clock source and tickless calibration once running.
        uint32_t const primask = __get_PRIMASK();
        __disable_irq();
        vPortSetupTimerInterrupt();
        __set_PRIMASK(primask);
        return true;
    }

    if (SysTick_Config(SystemCoreClock / 1000U) != 0U) {
        return false;
    }
    NVIC_SetPriority(SysTick_IRQn, 0U);
    return true;
}
