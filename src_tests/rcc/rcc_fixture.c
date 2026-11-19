// Run the L496 driver and ST's LL register accesses against peripheral registers in RAM.
#include <stdbool.h>
#include <stddef.h>

#define STM32L496xx
#define USE_FULL_LL_DRIVER
#define HSE_VALUE 12'000'000U
#include <stm32l4xx.h>

static RCC_TypeDef rcc;
static PWR_TypeDef pwr;
static FLASH_TypeDef flash;
rcc_test_systick_t rcc_test_systick;
#undef RCC
#undef PWR
#undef FLASH
#define RCC (&rcc)
#define PWR (&pwr)
#define FLASH (&flash)

#include <stm32l4xx_ll_pwr.h>
#include <stm32l4xx_ll_rcc.h>
#include <stm32l4xx_ll_system.h>
#include "../../3rdparty/st_stm32l4/src_hal/stm32l4xx_ll_rcc.c"

bool rcc_test_pll_locks;
bool rcc_test_can_switch;
bool rcc_test_fault_on_switch;
void rcc_test_fail_hse(void);
bool rcc_test_flash_accepts;
bool rcc_test_flash_can_lower;
uint32_t rcc_test_tick_reads;
uint32_t rcc_test_tick_inits;

static uint32_t pll_ready(void) {
    if (rcc_test_pll_locks && (rcc.CR & RCC_CR_PLLON)) {
        rcc.CR |= RCC_CR_PLLRDY;
    } else {
        rcc.CR &= ~RCC_CR_PLLRDY;
    }
    return LL_RCC_PLL_IsReady();
}

static void switch_clock(uint32_t source) {
    LL_RCC_SetSysClkSource(source);
    if (rcc_test_fault_on_switch) {
        rcc_test_fault_on_switch = false;
        rcc_test_fail_hse();
        return;
    }
    if (rcc_test_can_switch) {
        MODIFY_REG(rcc.CFGR, RCC_CFGR_SWS, source << RCC_CFGR_SWS_Pos);
    }
}

static void write_flash_latency(uint32_t latency) {
    if (rcc_test_flash_accepts && (latency == LL_FLASH_LATENCY_4 || rcc_test_flash_can_lower)) {
        LL_FLASH_SetLatency(latency);
    }
}

#define LL_RCC_PLL_IsReady pll_ready
#define LL_RCC_SetSysClkSource switch_clock
#define LL_FLASH_SetLatency write_flash_latency
// This vendor helper uses a literal 32-bit MMIO address. I2C mux selection is outside this fixture.
#define LL_RCC_SetI2CClockSource(source) ((void)(source))
#include "rcc/uni_hal_rcc_stm32l4.c"

bool uni_hal_systick_init(void) {
    ++rcc_test_tick_inits;
    SystemCoreClockUpdate();
    SysTick->LOAD = SystemCoreClock / 1000U - 1U;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | 3U;
    return true;
}

uint32_t uni_hal_systick_get_freq(void) {
    return 1000;
}

uint32_t uni_hal_systick_get_ms(void) {
    // A frozen tick is intentional, including when timeout.pll is non-zero.
    ++rcc_test_tick_reads;
    return 0;
}

void SystemCoreClockUpdate(void) {
    LL_RCC_ClocksTypeDef clocks;
    LL_RCC_GetSystemClocksFreq(&clocks);
    SystemCoreClock = clocks.HCLK_Frequency;
}

void uni_hal_pwr_stm_set_backup_access(bool enable) {
    (void)enable;
}

void rcc_test_reset(void) {
    rcc = (RCC_TypeDef){0};
    pwr.CR1 = LL_PWR_REGU_VOLTAGE_SCALE1;
    flash.ACR = 0;
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    rcc.CR = RCC_CR_MSIRDY | RCC_CR_MSIRGSEL | LL_RCC_MSIRANGE_6 |
        RCC_CR_HSERDY | RCC_CR_HSIRDY;
    rcc.CSR = RCC_CSR_LSIRDY;
    g_uni_hal_rcc_config = nullptr;
    g_uni_hal_rcc_status = (uni_hal_rcc_stm32l4_status_t){0};
    rcc_test_fault_on_switch = false;
    rcc_test_pll_locks = true;
    rcc_test_can_switch = true;
    rcc_test_flash_accepts = true;
    rcc_test_flash_can_lower = true;
    rcc_test_tick_reads = 0;
    rcc_test_tick_inits = 0;
}

void rcc_test_fail_hse(void) {
    // CSS switched to MSI (STOPWUCK = 0), disabled HSE and the HSE-fed system PLL.
    rcc.CR &= ~(RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY);
    MODIFY_REG(rcc.CFGR, RCC_CFGR_SW | RCC_CFGR_SWS, LL_RCC_SYS_CLKSOURCE_MSI);
    rcc.CIFR = RCC_CIFR_CSSF;
    NMI_Handler();
}

uint32_t rcc_test_m(void) {
    return 1 + (LL_RCC_PLL_GetDivider() >> RCC_PLLCFGR_PLLM_Pos);
}

uint32_t rcc_test_n(void) {
    return LL_RCC_PLL_GetN();
}

bool rcc_test_css_acknowledged(void) {
    return (rcc.CICR & RCC_CICR_CSSC) != 0;
}

bool rcc_test_requested_pll(void) {
    return (rcc.CFGR & RCC_CFGR_SW) == LL_RCC_SYS_CLKSOURCE_PLL;
}

void rcc_test_hsi_not_ready(void) {
    rcc.CR &= ~RCC_CR_HSIRDY;
}

void rcc_test_voltage_range2(void) {
    pwr.CR1 = LL_PWR_REGU_VOLTAGE_SCALE2;
}
