// Run the H743 driver, CMSIS clock calculation and ST LL on a Cortex-M7 with registers in SRAM.
#include <stm32h7xx.h>
#include <stdbool.h>

static RCC_TypeDef rcc;
static FLASH_TypeDef flash;
static PWR_TypeDef pwr;
static DBGMCU_TypeDef debug;
#undef DBGMCU
#define DBGMCU (&debug)
#undef RCC
#undef FLASH
#undef PWR
#define RCC (&rcc)
#define FLASH (&flash)
#define PWR (&pwr)

#include "../../3rdparty/st_stm32h7/src_device/system_stm32h7xx.c"
#include "../../3rdparty/st_stm32h7/src_hal/stm32h7xx_ll_rcc.c"
#include "model.h"

static uint32_t model_hse_ready(void) {
    MODIFY_REG(RCC->CR, RCC_CR_HSERDY, (RCC->CR & RCC_CR_HSEON) ? RCC_CR_HSERDY : 0U);
    return LL_RCC_HSE_IsReady();
}

static uint32_t model_pll_ready(void) {
    MODIFY_REG(RCC->CR, RCC_CR_PLL1RDY, (RCC->CR & RCC_CR_PLL1ON) ? RCC_CR_PLL1RDY : 0U);
    return LL_RCC_PLL1_IsReady();
}

static void model_switch(uint32_t source) {
    LL_RCC_SetSysClkSource(source);
    MODIFY_REG(RCC->CFGR, RCC_CFGR_SWS, source << RCC_CFGR_SWS_Pos);
}

#define LL_RCC_HSE_IsReady model_hse_ready
#define LL_RCC_PLL1_IsReady model_pll_ready
#define LL_RCC_SetSysClkSource model_switch
#define uni_hal_systick_init model_systick_init
#include "rcc/uni_hal_rcc_stm32h7.c"

bool uni_hal_flash_configure_latency_auto(uint32_t hz) { return hz != 0U; }
bool uni_hal_dwt_init(void) { return false; }
bool uni_hal_dwt_is_inited(void) { return false; }
uint32_t uni_hal_dwt_get_tick(void) { return 0; }
uint32_t uni_hal_dwt_timeout_ms_to_tick(uint32_t ms) { return ms; }
uint32_t uni_hal_dwt_compare(uint32_t start, uint32_t end) { return end - start; }
uint32_t uni_hal_systick_get_ms(void) { return 0; }
bool uni_hal_systick_init(void) { return true; }
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

void model_css(void) {
    // HSE failure while running from its PLL triggers the automatic switch to HSI.
    CLEAR_BIT(RCC->CR, RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLL1ON | RCC_CR_PLL1RDY);
    MODIFY_REG(RCC->CFGR, RCC_CFGR_SW | RCC_CFGR_SWS, LL_RCC_SYS_CLKSOURCE_HSI);
    RCC->CIFR = RCC_CIFR_HSECSSF;
    NMI_Handler();
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
