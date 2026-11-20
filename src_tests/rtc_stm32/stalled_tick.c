#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include <stm32h7xx.h>
#else
#include <stm32l4xx.h>
#endif

static RCC_TypeDef rcc;
static PWR_TypeDef pwr;
static RTC_TypeDef rtc;
#undef RCC
#undef PWR
#undef RTC
#define RCC (&rcc)
#define PWR (&pwr)
#define RTC (&rtc)

#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include <stm32h7xx_ll_cortex.h>
#include <stm32h7xx_ll_rtc.h>
#else
#include <stm32l4xx_ll_cortex.h>
#include <stm32l4xx_ll_rtc.h>
#endif

static unsigned polls, init_at, sync_at, lse_at;
static uint32_t model_tick(void) {
    ++polls;
    if ((rtc.ISR & RTC_ISR_INIT) && polls == init_at) {
        rtc.ISR |= RTC_ISR_INITF;
    }
    if (!(rtc.ISR & RTC_ISR_INIT) && polls == sync_at) {
        rtc.ISR |= RTC_ISR_RSF;
    }
    if (polls == lse_at) {
        rcc.BDCR |= RCC_BDCR_LSERDY;
    }
    return 0U;
}

// The acknowledgement flags are read-only in hardware.
#define LL_RTC_EnableInitMode(dev) SET_BIT((dev)->ISR, RTC_ISR_INIT)
#define LL_RTC_DisableInitMode(dev) CLEAR_BIT((dev)->ISR, RTC_ISR_INIT | RTC_ISR_INITF)
#define LL_RTC_ClearFlag_RS(dev) CLEAR_BIT((dev)->ISR, RTC_ISR_RSF)
#define uni_hal_systick_get_ms model_tick
#define LL_SYSTICK_IsActiveCounterFlag() model_tick() // COUNTFLAG stays clear while the RTC responds.
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include "../../3rdparty/st_stm32h7/src_hal/stm32h7xx_ll_rtc.c"
#else
#include "../../3rdparty/st_stm32l4/src_hal/stm32l4xx_ll_rtc.c"
#endif
#include "rtc/uni_hal_rtc_stm32.c"
#include "../arm/semihost_test.h"

static unsigned check(void) {
    SystemCoreClock = 1000U;
    rcc.BDCR = RCC_BDCR_LSEON | RCC_BDCR_LSERDY;
    uni_hal_rtc_context_t ctx = {.clock_source = UNI_HAL_RCC_CLKSRC_LSE};

    // Very late INITF is a failure even if the millisecond clock never advances.
    init_at = 30000U;
    CHECK(!uni_hal_rtc_init(&ctx));
    CHECK(!ctx.inited && !ctx.calendar_valid && polls < init_at);
    CHECK(!(rtc.ISR & RTC_ISR_INIT) && rtc.WPR == RTC_WRITE_PROTECTION_DISABLE);
    CHECK(rtc.PRER == 0U);
    polls = 0;
    init_at = 4U;
    CHECK(uni_hal_rtc_init(&ctx));
    CHECK(LL_RTC_GetSynchPrescaler(&rtc) == 255U && LL_RTC_GetAsynchPrescaler(&rtc) == 127U);

    uni_hal_rtc_datetime_t const next = {.year = 2026, .month = 11, .day = 20, .weekday = 5, .hours = 18};
    polls = 0;
    init_at = 30000U;
    CHECK(!uni_hal_rtc_set(&ctx, &next));
    CHECK(!rtc.DR && !rtc.TR && !ctx.calendar_valid && polls < init_at);
    CHECK(!(rtc.ISR & RTC_ISR_INIT) && rtc.WPR == RTC_WRITE_PROTECTION_DISABLE);

    polls = 0;
    init_at = 4U;
    sync_at = 30000U;
    CHECK(!uni_hal_rtc_set(&ctx, &next));
    CHECK(!ctx.calendar_valid && polls < sync_at);
    CHECK(!(rtc.ISR & RTC_ISR_INIT) && rtc.WPR == RTC_WRITE_PROTECTION_DISABLE);
    polls = 0;
    sync_at = 12U;
    CHECK(uni_hal_rtc_set(&ctx, &next) && ctx.calendar_valid);

    // The requested LSI is healthy. A failed LSE restart after backup reset
    // must not block the migration to LSI.
    polls = 0;
    lse_at = 30000U;
    rcc.BDCR = RCC_BDCR_LSEON | LL_RCC_RTC_CLKSOURCE_LSE;
    rcc.CSR = RCC_CSR_LSIRDY;
    rtc.ISR = RTC_ISR_INITF;
    ctx.clock_source_change = true;
    ctx.clock_source = UNI_HAL_RCC_CLKSRC_LSI;
    CHECK(uni_hal_rtc_init(&ctx));
    CHECK(polls < lse_at && !(rcc.BDCR & RCC_BDCR_LSERDY));
    CHECK(uni_hal_rtc_clock_source_get() == UNI_HAL_RCC_CLKSRC_LSI);
    return 0;
}
