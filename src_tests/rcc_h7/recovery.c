#include <stm32h7xx.h>
#include "model.h"
#include "systick/uni_hal_systick_stm32.h"

static unsigned notifications;
static uni_hal_rcc_stm32h7_status_t callback_status;
void uni_hal_rcc_css_callback(void) {
    ++notifications;
    callback_status = uni_hal_rcc_stm32h7_status_get();
}

unsigned model_test(void) {
    uni_hal_rcc_stm32h7_config_t config = model_config();
    CHECK(uni_hal_rcc_stm32h7_config_set(&config));
    model_inject_css = true; // CSS preempts the PLL1 SYSCLK request during the initial configuration.
    CHECK(uni_hal_rcc_init());
    CHECK(notifications == 1U && callback_status.css_fault && callback_status.css_recovery_pending);
    CHECK(!callback_status.inited && !callback_status.sys_inited && !callback_status.pll_inited[0]);
    uni_hal_rcc_stm32h7_status_t status = uni_hal_rcc_stm32h7_status_get();
    CHECK(status.css_fault && status.inited && status.sys_inited && status.pll_inited[0]);
    CHECK(!status.hse_inited && status.hsi_inited && SystemCoreClock == 100000000U);

    // Model a live RTOS timer at 250 Hz and an HSI divider of two.
    model_divide_hsi();
    uni_hal_systick_uses_rtos_period = true;
    model_systick.CTRL = SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk;
    model_systick.LOAD = 49999U;
    model_systick.VAL = 123U;
    unsigned const flash_calls = model_flash_calls;
    model_flash_allowed = false;
    model_css();
    CHECK(notifications == 2U && !callback_status.css_recovery_pending && callback_status.inited);
    CHECK(uni_hal_rcc_is_inited() && SystemCoreClock == 100000000U);
    CHECK(model_systick.CTRL == 3U && model_systick.LOAD == 49999U && model_systick.VAL == 123U);

    // No task, SysTick or DWT can advance the timeout while this exception is active.
    model_pll_locks = false;
    model_css();
    status = uni_hal_rcc_stm32h7_status_get();
    CHECK(notifications == 3U && !callback_status.css_recovery_pending && !callback_status.inited);
    CHECK(!status.inited && !status.pll_inited[0] && status.sys_inited && status.hsi_inited);
    CHECK(SystemCoreClock == 32000000U && model_systick.LOAD == 15999U && model_systick.CTRL == 3U);
    CHECK(model_systick.VAL == 0U && model_clock_reads == 0U && model_flash_calls == flash_calls);

    // A failed PLL switch must not remain armed after recovery has returned.
    model_pll_locks = true;
    model_pll_switches = false;
    model_css();
    status = uni_hal_rcc_stm32h7_status_get();
    CHECK(!status.inited && status.sys_inited && status.hsi_inited);
    CHECK(SystemCoreClock == 32000000U && model_systick.LOAD == 15999U);
    CHECK(model_requested_clock() == RCC_CFGR_SW_HSI);
    // If NMI wins the claim, init observes its result without restarting the clocks.
    model_pll_switches = true;
    model_css_on_claim = true;
    CHECK(uni_hal_rcc_init());
    CHECK(notifications == 5U && callback_status.inited && !callback_status.css_recovery_pending);
    CHECK(model_flash_calls == flash_calls && SystemCoreClock == 100000000U);

    // If init wins the claim, its finalization must consume the deferred recovery.
    model_css_after_claim = true;
    CHECK(uni_hal_rcc_init());
    CHECK(notifications == 6U && !callback_status.inited && callback_status.css_recovery_pending);
    CHECK(!uni_hal_rcc_stm32h7_status_get().css_recovery_pending);

    model_change_ahb_divider();
    model_css();
    CHECK(!uni_hal_rcc_is_inited() && SystemCoreClock == 32000000U);
    CHECK(model_requested_clock() == RCC_CFGR_SW_HSI);

    // A later ordinary initialization can prepare Flash and bus dividers and use HSE again.
    model_flash_allowed = true;
    CHECK(uni_hal_rcc_init());
    status = uni_hal_rcc_stm32h7_status_get();
    CHECK(status.hse_inited && status.pll_inited[0] && status.css_fault && !status.css_recovery_pending);
    CHECK(SystemCoreClock == 100000000U);
    return 0;
}
