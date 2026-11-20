#include <stm32h7xx.h>
#include "../arm/semihost_test.h"

static RCC_TypeDef rcc;
static PWR_TypeDef pwr;
static FLASH_TypeDef flash;
#undef RCC_BASE
#define RCC_BASE ((uintptr_t)&rcc)
#undef PWR_BASE
#define PWR_BASE ((uintptr_t)&pwr)
#undef FLASH_R_BASE
#define FLASH_R_BASE ((uintptr_t)&flash)
// External audio master clock used by the sub-Hz threshold case below.
#undef HSE_VALUE
#define HSE_VALUE 45158400U
#include "flash/uni_hal_flash_stm32h7.c"

static bool latency_is(uint32_t latency, uint32_t write_timing) {
    return LL_FLASH_GetLatency() == latency && (flash.ACR & FLASH_ACR_WRHIGHFREQ) == write_timing;
}

static unsigned check(void) {
    // A 400 MHz PLL1P sourced from HSI64. In VOS1, 200/100/50 MHz AXI need 2/1/0 wait states.
    rcc.CR = RCC_CR_HSION | RCC_CR_HSIRDY | RCC_CR_PLL1ON | RCC_CR_PLL1RDY;
    LL_RCC_PLL_SetSource(LL_RCC_PLLSOURCE_HSI);
    LL_RCC_PLL1_SetM(16U);
    LL_RCC_PLL1_SetN(200U);
    LL_RCC_PLL1_SetP(2U);
    LL_RCC_PLL1P_Enable();
    rcc.CFGR = LL_RCC_SYS_CLKSOURCE_STATUS_PLL1;
    pwr.D3CR = LL_PWR_REGU_VOLTAGE_SCALE1;

    LL_RCC_SetAHBPrescaler(LL_RCC_AHB_DIV_2);
    CHECK(uni_hal_flash_configure_latency_auto(0U) && latency_is(LL_FLASH_LATENCY_2, FLASH_ACR_WRHIGHFREQ_1));
    LL_RCC_SetSysPrescaler(LL_RCC_SYSCLK_DIV_2);
    CHECK(uni_hal_flash_configure_latency_auto(0U) && latency_is(LL_FLASH_LATENCY_1, FLASH_ACR_WRHIGHFREQ_0));
    LL_RCC_SetSysPrescaler(LL_RCC_SYSCLK_DIV_4);
    CHECK(uni_hal_flash_configure_latency_auto(0U) && latency_is(LL_FLASH_LATENCY_0, 0U));

    // A caller can also supply the prospective AXI clock before switching the hardware.
    CHECK(uni_hal_flash_configure_latency_auto(200000000U) && latency_is(LL_FLASH_LATENCY_2, FLASH_ACR_WRHIGHFREQ_1));
    LL_RCC_SetAHBPrescaler(LL_RCC_AHB_DIV_1);
    CHECK(uni_hal_flash_configure_latency_auto(0U) && latency_is(LL_FLASH_LATENCY_1, FLASH_ACR_WRHIGHFREQ_0));
    // HSI64 / 16 * (140 + FRACN / 8192) / 2 / 2 gives 140 or 140.5 MHz AXI.
    // The programmed fraction stays in PLL1FRACR when fractional mode is disabled.
    LL_RCC_SetSysPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAHBPrescaler(LL_RCC_AHB_DIV_2);
    LL_RCC_PLL1_SetN(140U);
    LL_RCC_PLL1_SetFRACN(4096U);
    LL_RCC_PLL1FRACN_Enable();
    CHECK(uni_hal_flash_configure_latency_auto(0U) && latency_is(LL_FLASH_LATENCY_2, FLASH_ACR_WRHIGHFREQ_0));
    LL_RCC_PLL1FRACN_Disable();
    CHECK(uni_hal_flash_configure_latency_auto(0U) && latency_is(LL_FLASH_LATENCY_1, FLASH_ACR_WRHIGHFREQ_0));

    // HSI64 / 51 * (223 + 1024/8192) / 2 is exactly 140 MHz.
    // One more fractional step raises AXI by 76.59 Hz and requires another wait state.
    LL_RCC_PLL1_SetM(51U);
    LL_RCC_PLL1_SetN(223U);
    LL_RCC_PLL1_SetVCOInputRange(LL_RCC_PLLINPUTRANGE_1_2);
    LL_RCC_PLL1_SetVCOOutputRange(LL_RCC_PLLVCORANGE_MEDIUM);
    LL_RCC_SetAHBPrescaler(LL_RCC_AHB_DIV_1);
    LL_RCC_PLL1FRACN_Enable();
    LL_RCC_PLL1_SetFRACN(1024U);
    CHECK(uni_hal_flash_configure_latency_auto(0U) && latency_is(LL_FLASH_LATENCY_1, FLASH_ACR_WRHIGHFREQ_0));
    LL_RCC_PLL1_SetFRACN(1025U);
    CHECK(uni_hal_flash_configure_latency_auto(0U) && latency_is(LL_FLASH_LATENCY_2, FLASH_ACR_WRHIGHFREQ_0));

    // HSE / 37 * (180 + 2084/8192) / 2 / 2 = 55,000,000.338 Hz at VOS2.
    // Exercise each bus divider separately; either can hide the fractional hertz.
    rcc.CR |= RCC_CR_HSEON | RCC_CR_HSERDY;
    LL_RCC_PLL_SetSource(LL_RCC_PLLSOURCE_HSE);
    LL_RCC_PLL1_SetM(37U);
    LL_RCC_PLL1_SetN(180U);
    pwr.D3CR = LL_PWR_REGU_VOLTAGE_SCALE2;
    LL_RCC_SetAHBPrescaler(LL_RCC_AHB_DIV_2);
    LL_RCC_PLL1_SetFRACN(2083U);
    CHECK(uni_hal_flash_configure_latency_auto(0U) && latency_is(LL_FLASH_LATENCY_0, 0U));
    LL_RCC_PLL1_SetFRACN(2084U);
    CHECK(uni_hal_flash_configure_latency_auto(0U) && latency_is(LL_FLASH_LATENCY_1, FLASH_ACR_WRHIGHFREQ_0));
    LL_RCC_SetAHBPrescaler(LL_RCC_AHB_DIV_1);
    LL_RCC_SetSysPrescaler(LL_RCC_SYSCLK_DIV_2);
    CHECK(uni_hal_flash_configure_latency_auto(0U) && latency_is(LL_FLASH_LATENCY_1, FLASH_ACR_WRHIGHFREQ_0));
    return 0;
}
