#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include <stm32h7xx.h>
#else
#include <stm32l4xx.h>
#endif

static RTC_TypeDef rtc;
static RCC_TypeDef rcc;
static PWR_TypeDef pwr;
#undef RTC
#undef RCC
#undef PWR
#define RTC (&rtc)
#define RCC (&rcc)
#define PWR (&pwr)

#include "rtc/uni_hal_rtc_stm32.c"
#include "../arm/semihost_test.h"

static unsigned check(void) {
    // A previous firmware left a running clock at 11:58:37 PM on 2026-11-20.
    rcc.BDCR = LL_RCC_RTC_CLKSOURCE_LSE | RCC_BDCR_RTCEN | RCC_BDCR_LSERDY;
    rtc.ISR = RTC_ISR_INITS | RTC_ISR_INITF;
    rtc.CR = RTC_CR_FMT | RTC_CR_BYPSHAD;
    rtc.PRER = 255U;
    LL_RTC_DATE_Config(&rtc, 5U, 0x20U, 0x11U, 0x26U);
    LL_RTC_TIME_Config(&rtc, LL_RTC_TIME_FORMAT_PM, 0x11U, 0x58U, 0x37U);
    uni_hal_rtc_context_t ctx = {.clock_source = UNI_HAL_RCC_CLKSRC_LSE};
    CHECK(uni_hal_rtc_init(&ctx) && ctx.calendar_valid);
    CHECK(rtc.CR & RTC_CR_FMT);
    uni_hal_rtc_datetime_t time;
    CHECK(uni_hal_rtc_get(&ctx, &time));
    CHECK(time.hours == 23 && time.minutes == 58 && time.seconds == 37);
    CHECK(time.year == 2026 && time.month == 11 && time.day == 20 && time.weekday == 5);

    // Compare the register encoding independently of the public get/set round trip.
    static const struct {
        uint8_t hour, hour_bcd_12, hour_bcd_24;
        bool pm;
    } cases[] = {{0, 0x12, 0x00, false}, {1, 0x01, 0x01, false}, {11, 0x11, 0x11, false},
                 {12, 0x12, 0x12, true}, {13, 0x01, 0x13, true}, {23, 0x11, 0x23, true}};
    for (unsigned mode = 0; mode < 2; ++mode) {
        bool const am_pm = mode == 0;
        LL_RTC_SetHourFormat(&rtc, am_pm ? LL_RTC_HOURFORMAT_AMPM : LL_RTC_HOURFORMAT_24HOUR);
        for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
            time.hours = cases[i].hour;
            CHECK(uni_hal_rtc_set(&ctx, &time));
            CHECK(LL_RTC_TIME_GetHour(&rtc) == (am_pm ? cases[i].hour_bcd_12 : cases[i].hour_bcd_24));
            CHECK((LL_RTC_TIME_GetFormat(&rtc) == LL_RTC_TIME_FORMAT_PM) == (am_pm && cases[i].pm));
            CHECK(uni_hal_rtc_get(&ctx, &time) && time.hours == cases[i].hour);
        }
    }
    return 0;
}
