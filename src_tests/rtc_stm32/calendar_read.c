#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include <stm32h7xx_ll_rtc.h>
#else
#include <stm32l4xx_ll_rtc.h>
#endif
#include <stdbool.h>

static RTC_TypeDef rtc;
#undef RTC
#define RTC (&rtc)

static bool midnight_pending, keep_changing, shadow_locked;
static uint32_t subsecond;

static uint32_t read_subsecond(RTC_TypeDef *dev) {
    shadow_locked = !LL_RTC_IsShadowRegBypassEnabled(dev);
    if (keep_changing) {
        subsecond = (subsecond - 1U) & 255U;
    }
    return subsecond;
}

static uint32_t read_date(RTC_TypeDef *dev) {
    // At midnight DR can advance after TR was read in bypass mode or on a missed shadow lock.
    if (midnight_pending) {
        midnight_pending = false;
        LL_RTC_TIME_Config(dev, LL_RTC_TIME_FORMAT_AM_OR_24, 0, 0, 0);
        LL_RTC_DATE_Config(dev, 5U, 0x20U, 0x11U, 0x26U);
        subsecond = 255U;
    }
    shadow_locked = false;
    return LL_RTC_DATE_Get(dev);
}

#define LL_RTC_TIME_GetSubSecond read_subsecond
#define LL_RTC_DATE_Get read_date
#include "rtc/uni_hal_rtc_stm32.c"
#include "../arm/semihost_test.h"

static unsigned check(void) {
    uni_hal_rtc_context_t ctx = {.inited = true};
    rtc.PRER = 255U;
    // A system reset cleared the shadows while the calendar kept running.
    rtc.DR = 0x00002101U;
    rtc.ISR = RTC_ISR_INITS;
    uni_hal_rtc_datetime_t observed = {.year = 2099U, .milliseconds = 789U};
    CHECK(!uni_hal_rtc_get(&ctx, &observed));
    CHECK(observed.year == 2099U && observed.milliseconds == 789U);
    for (unsigned bypass = 0; bypass <= 1; ++bypass) {
        rtc.CR = bypass ? RTC_CR_BYPSHAD : 0U;
        // Shadow reads can resume after the first copy. Direct reads do not use RSF.
        rtc.ISR = RTC_ISR_INITS | (bypass ? 0U : RTC_ISR_RSF);
        LL_RTC_DATE_Config(&rtc, 4U, 0x19U, 0x11U, 0x26U);
        LL_RTC_TIME_Config(&rtc, LL_RTC_TIME_FORMAT_AM_OR_24, 0x23U, 0x59U, 0x59U);
        midnight_pending = true;
        subsecond = 0U;
        CHECK(uni_hal_rtc_get(&ctx, &observed));
        CHECK(observed.day == 20U && observed.hours == 0U && observed.seconds == 0U && observed.milliseconds == 0U);
        CHECK(!shadow_locked);

        keep_changing = true;
        observed.day = 15U;
        observed.milliseconds = 345U;
        CHECK(!uni_hal_rtc_get(&ctx, &observed));
        CHECK(observed.day == 15U && observed.milliseconds == 345U && !shadow_locked);
        keep_changing = false;
        CHECK(uni_hal_rtc_get(&ctx, &observed) && observed.day == 20U);
    }
    return 0;
}
