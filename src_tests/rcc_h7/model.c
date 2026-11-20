// Run the H743 driver, CMSIS clock calculation and ST LL on a Cortex-M7 with registers in SRAM.
#include <stm32h7xx.h>
#include <stdbool.h>

RCC_TypeDef model_rcc;
static FLASH_TypeDef flash;
static PWR_TypeDef pwr;
static DBGMCU_TypeDef debug;
#undef DBGMCU
#define DBGMCU (&debug)
#undef RCC
#undef FLASH
#undef PWR
#define RCC (&model_rcc)
#define FLASH (&flash)
#define PWR (&pwr)

#include "../../3rdparty/st_stm32h7/src_device/system_stm32h7xx.c"
#include "../../3rdparty/st_stm32h7/src_hal/stm32h7xx_ll_rcc.c"
#include "model.h"
#include "dwt/uni_hal_dwt_internal.h"

SysTick_Type model_systick;
#undef SysTick
#define SysTick (&model_systick)
bool model_pll_locks = true, model_pll_switches = true, model_inject_css;
bool model_css_active, model_flash_allowed = true;
bool model_css_on_claim, model_css_after_claim;
unsigned model_clock_reads, model_flash_calls, model_pll_delay;
bool model_have_dwt;
uint32_t model_ms_step, model_dwt_step;

static uint32_t model_hse_ready(void) {
    MODIFY_REG(RCC->CR, RCC_CR_HSERDY, (RCC->CR & RCC_CR_HSEON) ? RCC_CR_HSERDY : 0U);
    return LL_RCC_HSE_IsReady();
}

static uint32_t model_pll_ready(void) {
    if ((RCC->CR & RCC_CR_PLL1ON) && model_pll_delay != 0U) {
        --model_pll_delay;
        return 0U;
    }
    MODIFY_REG(RCC->CR, RCC_CR_PLL1RDY, (model_pll_locks && (RCC->CR & RCC_CR_PLL1ON)) ? RCC_CR_PLL1RDY : 0U);
    return LL_RCC_PLL1_IsReady();
}

static void model_switch(uint32_t source) {
    LL_RCC_SetSysClkSource(source);
    if (source == LL_RCC_SYS_CLKSOURCE_PLL1 && model_inject_css) {
        model_inject_css = false;
        model_css();
    } else if (source != LL_RCC_SYS_CLKSOURCE_PLL1 || model_pll_switches) {
        MODIFY_REG(RCC->CFGR, RCC_CFGR_SWS, source << RCC_CFGR_SWS_Pos);
    }
}

static uint32_t model_irq_pause(void) {
    uint32_t const mask = uni_hal_core_irq_pause();
    if (model_css_on_claim) {
        model_css_on_claim = false;
        model_css();
    }
    return mask;
}

static void model_irq_resume(uint32_t mask) {
    if (model_css_after_claim) {
        model_css_after_claim = false;
        model_css();
    }
    uni_hal_core_irq_resume(mask);
}

#define uni_hal_core_irq_pause model_irq_pause
#define uni_hal_core_irq_resume model_irq_resume
#define LL_RCC_HSE_IsReady model_hse_ready
#define LL_RCC_PLL1_IsReady model_pll_ready
#define LL_RCC_SetSysClkSource model_switch
#define uni_hal_systick_reclock model_systick_reclock
#define uni_hal_systick_sleep model_systick_sleep
#include "systick/uni_hal_systick_reclock_stm32.c"
#define uni_hal_systick_init model_systick_init
#include "rcc/uni_hal_rcc_stm32h7.c"

bool uni_hal_flash_configure_latency_auto(uint32_t hz) { ++model_flash_calls; return model_flash_allowed && hz != 0U; }
bool uni_hal_dwt_init(void) { return false; }
bool uni_hal_dwt_is_inited(void) { return model_have_dwt; }
uint32_t uni_hal_dwt_get_tick(void) {
    static uint32_t cycles;
    cycles += model_dwt_step;
    return cycles;
}
uint32_t uni_hal_dwt_timeout_ms_to_tick(uint32_t ms) {
    return uni_hal_dwt_internal_timeout_ms_to_tick(ms, SystemCoreClock);
}
uint32_t uni_hal_dwt_compare(uint32_t start, uint32_t end) { return end - start; }
uint32_t uni_hal_systick_get_ms(void) {
    static uint32_t ms;
    if (model_css_active) ++model_clock_reads;
    ms += model_ms_step;
    return ms;
}
bool uni_hal_systick_init(void) {
    SystemCoreClockUpdate();
    model_systick.LOAD = SystemCoreClock / 1000U - 1U;
    return true;
}
void uni_hal_pwr_stm_set_backup_access(bool val) { (void)val; }

uni_hal_rcc_stm32h7_config_t model_config(void) {
    RCC->CR = RCC_CR_HSION | RCC_CR_HSIRDY;
    RCC->CSR = RCC_CSR_LSIRDY;
    return (uni_hal_rcc_stm32h7_config_t){
        .hse_enable = true, .hse_css = true,
        .pll = {{.m = HSE_VALUE / 2000000U, .n = 100U, .p = 2U}},
        .timeout = {.pll = 2U}
    };
}

bool model_hsi_enabled(void) { return (RCC->CR & RCC_CR_HSION) != 0U; }

void model_divide_hsi(void) {
    LL_RCC_HSI_SetDivider(LL_RCC_HSI_DIV2);
}

void model_change_ahb_divider(void) {
    LL_RCC_SetAHBPrescaler(LL_RCC_AHB_DIV_1);
}

uint32_t model_requested_clock(void) { return RCC->CFGR & RCC_CFGR_SW; }

void model_css(void) {
    // HSE failure while running from its PLL triggers the automatic switch to HSI.
    CLEAR_BIT(RCC->CR, RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLL1ON | RCC_CR_PLL1RDY);
    MODIFY_REG(RCC->CFGR, RCC_CFGR_SW | RCC_CFGR_SWS, LL_RCC_SYS_CLKSOURCE_HSI);
    RCC->CIFR = RCC_CIFR_HSECSSF;
    model_css_active = true;
    NMI_Handler();
    model_css_active = false;
}

extern char _stack_top;
void run_tests(void);
__attribute__((used, section(".test_vectors")))
const uintptr_t vectors[] = {(uintptr_t)&_stack_top, (uintptr_t)run_tests};
void run_tests(void) {
    SCB->CPACR |= (3UL << 20) | (3UL << 22);
    __DSB();
    __ISB();
    uint32_t result[] = {0x20026U, model_test()};
    register uint32_t op __asm__("r0") = 0x20U;
    register void *arg __asm__("r1") = result;
    __asm__ volatile("bkpt 0xab" : "+r"(op), "+r"(arg) :: "memory");
    for (;;) {}
}
