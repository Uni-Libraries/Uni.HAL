#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include <stm32h7xx_ll_rtc.h>
#else
#include <stm32l4xx_ll_rtc.h>
#endif
#include <stdbool.h>

static RTC_TypeDef rtc;
#undef RTC
#define RTC (&rtc)

static bool advance_second, grant_init = true;
static bool shadow_ready;
static unsigned restarts, sync_reads;

static void resume_calendar(RTC_TypeDef *dev) {
    CLEAR_BIT(dev->ISR, RTC_ISR_INIT | RTC_ISR_INITF);
    ++restarts;
    if (advance_second) {
        // The calling task is preempted for one second the first time the clock runs again.
        advance_second = false;
        if (LL_RTC_TIME_Get(dev) == 0x235959U) {
            LL_RTC_TIME_Config(dev, LL_RTC_TIME_FORMAT_AM_OR_24, 0, 0, 0);
            // The dates in this test are within a single month and week.
            uint32_t day = __LL_RTC_CONVERT_BCD2BIN(LL_RTC_DATE_GetDay(dev)) + 1U;
            LL_RTC_DATE_SetDay(dev, __LL_RTC_CONVERT_BIN2BCD(day));
            LL_RTC_DATE_SetWeekDay(dev, LL_RTC_DATE_GetWeekDay(dev) + 1U);
        } else {
            LL_RTC_TIME_SetSecond(dev, 1U); // requested time is 14:15:00
        }
    }
}

static uint32_t model_tick(void) {
    static uint32_t ms;
    if (rtc.ISR & RTC_ISR_INIT) {
        if (grant_init) {
            rtc.ISR |= RTC_ISR_INITF;
        }
    } else if (++sync_reads > 1U && shadow_ready) {
        rtc.ISR |= RTC_ISR_RSF;
    }
    return ++ms;
}

#define uni_hal_systick_get_ms model_tick
#define LL_RTC_EnableInitMode(dev) SET_BIT((dev)->ISR, RTC_ISR_INIT)
#define LL_RTC_DisableInitMode resume_calendar
#define LL_RTC_ClearFlag_RS(dev) CLEAR_BIT((dev)->ISR, RTC_ISR_RSF)
#include "rtc/uni_hal_rtc_stm32.c"
#include "../arm/semihost_test.h"

static unsigned check(void) {
    rtc.CR = RTC_CR_BYPSHAD;
    LL_RTC_DATE_Config(&rtc, 3U, 0x18U, 0x11U, 0x26U);
    LL_RTC_TIME_Config(&rtc, LL_RTC_TIME_FORMAT_AM_OR_24, 0x23U, 0x59U, 0x59U);
    uni_hal_rtc_context_t ctx = {.inited = true};
    const uni_hal_rtc_datetime_t next = {.year = 2026, .month = 11, .day = 20, .weekday = 5,
                                         .hours = 14, .minutes = 15};
    advance_second = true;
    CHECK(uni_hal_rtc_set(&ctx, &next));
    uni_hal_rtc_datetime_t readback;
    CHECK(uni_hal_rtc_get(&ctx, &readback));
    CHECK(readback.day == next.day && readback.weekday == next.weekday);
    CHECK(readback.hours == 14U && readback.minutes == 15U && readback.seconds == 1U);
    CHECK(restarts == 1U && ctx.calendar_valid && rtc.WPR == RTC_WRITE_PROTECTION_DISABLE);

    uint32_t const old_time = rtc.TR, old_date = rtc.DR;
    uni_hal_rtc_datetime_t invalid = next;
    invalid.month = 4U;
    invalid.day = 31U;
    CHECK(!uni_hal_rtc_set(&ctx, &invalid));
    invalid.year = 2025U;
    invalid.month = 2U;
    invalid.day = 29U;
    CHECK(!uni_hal_rtc_set(&ctx, &invalid));
    CHECK(rtc.TR == old_time && rtc.DR == old_date && ctx.calendar_valid && restarts == 1U);
    ctx.calendar_valid = false;
    grant_init = false;
    CHECK(!uni_hal_rtc_set(&ctx, &next));
    CHECK(rtc.TR == old_time && rtc.DR == old_date && !ctx.calendar_valid);
    CHECK(!(rtc.ISR & RTC_ISR_INIT) && rtc.WPR == RTC_WRITE_PROTECTION_DISABLE);

    grant_init = true;
    rtc.CR = 0U; // Shadow synchronization never completes.
    CHECK(!uni_hal_rtc_set(&ctx, &next) && !ctx.calendar_valid);
    CHECK(!(rtc.ISR & RTC_ISR_INIT) && rtc.WPR == RTC_WRITE_PROTECTION_DISABLE);
    shadow_ready = true;
    sync_reads = 0U;
    CHECK(uni_hal_rtc_set(&ctx, &next) && ctx.calendar_valid);
    CHECK(sync_reads >= 2U && rtc.WPR == RTC_WRITE_PROTECTION_DISABLE);
    sync_reads = 0U;
    uni_hal_rtc_datetime_t leap_day = next;
    leap_day.year = 2024U;
    leap_day.month = 2U;
    leap_day.day = 29U;
    CHECK(uni_hal_rtc_set(&ctx, &leap_day) && uni_hal_rtc_get(&ctx, &readback));
    CHECK(readback.year == 2024U && readback.month == 2U && readback.day == 29U);
    return 0;
}
