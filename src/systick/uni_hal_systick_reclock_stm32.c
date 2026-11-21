#include "uni_hal_target.h"

#if defined(UNI_HAL_TARGET_STM32H7)
#include <stm32h7xx.h>
#else
#include <stm32l4xx.h>
#endif
#include <FreeRTOS.h>
#include <task.h>

#include "systick/uni_hal_systick_stm32.h"

#ifdef UNI_HAL_STM32_TICKLESS_SYSTICK
static volatile bool port_sleep_active;
static volatile bool port_reclock_needed;

extern void vPortSetupTimerInterrupt(void);
extern void vPortSuppressTicksAndSleep(TickType_t ticks);

// Only the port knows its one-tick reload, maximum sleep and wakeup compensation.
// An NMI during setup requests one more pass with the new clock.
static void refresh_port_timer(void) {
    while (port_reclock_needed) {
        port_reclock_needed = false;
        vPortSetupTimerInterrupt();
    }
}

void uni_hal_systick_sleep(uint32_t ticks) {
    uint32_t const primask = __get_PRIMASK();
    __disable_irq();
    port_sleep_active = true;
    refresh_port_timer();
    vPortSuppressTicksAndSleep(ticks);

    // The port returns with its regular reload restored and interrupts enabled.
    __disable_irq();
    port_sleep_active = false;
    refresh_port_timer();
    __set_PRIMASK(primask);
}
#endif

void uni_hal_systick_reclock(uint32_t old_core_hz, uint32_t new_core_hz) {
    if (old_core_hz == new_core_hz || old_core_hz == 0U) {
        return;
    }
#ifdef UNI_HAL_STM32_TICKLESS_SYSTICK
    if (uni_hal_systick_uses_rtos_period) {
        port_reclock_needed = true;
        if (port_sleep_active) {
            // Wake WFI even if CSS arrived just before it. Leave the temporary long
            // reload and current count intact until the port measures the elapsed sleep.
            SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
            return;
        }
    }
#endif
    // Both SysTick sources scale with HCLK. Preserve the current owner's clock source,
    // interrupt priority and enable bits, including when the timer is stopped.
    uint32_t const old_reload = SysTick->LOAD;
    if (old_reload == 0U) {
        return;
    }
    uint64_t scaled = (uint64_t)(old_reload + 1U) * new_core_hz;
    uint64_t period = (scaled + old_core_hz / 2U) / old_core_hz;
    if (period > SysTick_LOAD_RELOAD_Msk + 1U) {
        period = SysTick_LOAD_RELOAD_Msk + 1U;
    }
    SysTick->LOAD = period > 1U ? (uint32_t)period - 1U : 1U;
    SysTick->VAL = 0;
}
