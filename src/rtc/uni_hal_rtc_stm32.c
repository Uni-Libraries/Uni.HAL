//
// Includes
//

// stdlib
#include <stddef.h>

// ST
#if defined(UNI_HAL_TARGET_MCU_STM32L496)
    #include <stm32l4xx_ll_bus.h>
    #include <stm32l4xx_ll_pwr.h>
    #include <stm32l4xx_ll_rcc.h>
    #include <stm32l4xx_ll_rtc.h>
#elif defined(UNI_HAL_TARGET_MCU_STM32H743)
    #include <stm32h7xx_ll_bus.h>
    #include <stm32h7xx_ll_pwr.h>
    #include <stm32h7xx_ll_rcc.h>
    #include <stm32h7xx_ll_rtc.h>
#else
    #error "unknown MCU"
#endif

// Uni.HAL
#include "rtc/uni_hal_rtc.h"



//
// Defines
//

enum {
    /** the calendar registers hold two digits of the year */
    UNI_HAL_RTC_YEAR_BASE = 2000U,

    /** 1 Hz from the 32.768 kHz crystal: 32768 / (127 + 1) / (255 + 1) */
    UNI_HAL_RTC_LSE_PREDIV_A = 127U,
    UNI_HAL_RTC_LSE_PREDIV_S = 255U,

    /** 1 Hz from the 32 kHz internal oscillator: 32000 / (127 + 1) / (249 + 1) */
    UNI_HAL_RTC_LSI_PREDIV_A = 127U,
    UNI_HAL_RTC_LSI_PREDIV_S = 249U,
};



//
// Private
//

static void _uni_hal_rtc_bus_clock_enable(void) {
#if defined(UNI_HAL_TARGET_MCU_STM32L496)
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_RTCAPB);
#else
    LL_APB4_GRP1_EnableClock(LL_APB4_GRP1_PERIPH_RTCAPB);
#endif
}

static bool _uni_hal_rtc_datetime_is_valid(const uni_hal_rtc_datetime_t *datetime) {
    return datetime != NULL &&
           datetime->year >= UNI_HAL_RTC_YEAR_BASE && datetime->year < (UNI_HAL_RTC_YEAR_BASE + 100U) &&
           datetime->month >= 1U && datetime->month <= 12U &&
           datetime->day >= 1U && datetime->day <= 31U &&
           datetime->weekday >= 1U && datetime->weekday <= 7U &&
           datetime->hours <= 23U && datetime->minutes <= 59U && datetime->seconds <= 59U;
}



//
// Public
//

uni_hal_rcc_clksrc_e uni_hal_rtc_clock_source_get(void) {
    uni_hal_rcc_clksrc_e result;
    switch (LL_RCC_GetRTCClockSource()) {
    case LL_RCC_RTC_CLKSOURCE_LSE:
        result = UNI_HAL_RCC_CLKSRC_LSE;
        break;
    case LL_RCC_RTC_CLKSOURCE_LSI:
        result = UNI_HAL_RCC_CLKSRC_LSI;
        break;
    case LL_RCC_RTC_CLKSOURCE_NONE:
        result = UNI_HAL_RCC_CLKSRC_NONE;
        break;
    default:
        // HSE divided down
        result = UNI_HAL_RCC_CLKSRC_HSE_DIV_32;
        break;
    }
    return result;
}


bool uni_hal_rtc_init(uni_hal_rtc_context_t *ctx) {
    bool result = false;

    if (ctx != NULL) {
        _uni_hal_rtc_bus_clock_enable();
        LL_PWR_EnableBkUpAccess();

        // The clock selection is write-once until the backup domain is reset. Select a source
        // only when there is none, so that a running calendar is never disturbed.
        result = true;
        if (LL_RCC_GetRTCClockSource() == LL_RCC_RTC_CLKSOURCE_NONE) {
            switch (ctx->clock_source) {
            case UNI_HAL_RCC_CLKSRC_LSE:
                result = LL_RCC_LSE_IsReady() != 0U;
                if (result) {
                    LL_RCC_SetRTCClockSource(LL_RCC_RTC_CLKSOURCE_LSE);
                }
                break;
            case UNI_HAL_RCC_CLKSRC_LSI:
                result = LL_RCC_LSI_IsReady() != 0U;
                if (result) {
                    LL_RCC_SetRTCClockSource(LL_RCC_RTC_CLKSOURCE_LSI);
                }
                break;
            default:
                result = false;
                break;
            }
        }

        if (result) {
            LL_RCC_EnableRTC();

            ctx->calendar_valid = LL_RTC_IsActiveFlag_INITS(RTC) != 0U;
            if (!ctx->calendar_valid) {
                // first start: set the prescalers for the clock in use; the calendar starts at
                // its reset value and INITS stays clear until uni_hal_rtc_set() is called
                bool const from_lsi = LL_RCC_GetRTCClockSource() == LL_RCC_RTC_CLKSOURCE_LSI;
                LL_RTC_InitTypeDef init = {
                    .HourFormat = LL_RTC_HOURFORMAT_24HOUR,
                    .AsynchPrescaler = from_lsi ? UNI_HAL_RTC_LSI_PREDIV_A : UNI_HAL_RTC_LSE_PREDIV_A,
                    .SynchPrescaler = from_lsi ? UNI_HAL_RTC_LSI_PREDIV_S : UNI_HAL_RTC_LSE_PREDIV_S,
                };
                result = LL_RTC_Init(RTC, &init) == SUCCESS;
            }
        }

        ctx->inited = result;
    }

    return result;
}


bool uni_hal_rtc_is_inited(const uni_hal_rtc_context_t *ctx) {
    return ctx != NULL && ctx->inited;
}


bool uni_hal_rtc_get(const uni_hal_rtc_context_t *ctx, uni_hal_rtc_datetime_t *datetime) {
    bool result = false;

    if (uni_hal_rtc_is_inited(ctx) && datetime != NULL) {
        // reading the time latches the date until the date register is read: this order gives
        // a consistent pair
        uint32_t const time = LL_RTC_TIME_Get(RTC);
        uint32_t const date = LL_RTC_DATE_Get(RTC);

        datetime->hours = (uint8_t)__LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_HOUR(time));
        datetime->minutes = (uint8_t)__LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_MINUTE(time));
        datetime->seconds = (uint8_t)__LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_SECOND(time));
        datetime->year = (uint16_t)(UNI_HAL_RTC_YEAR_BASE + __LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_YEAR(date)));
        datetime->month = (uint8_t)__LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_MONTH(date));
        datetime->day = (uint8_t)__LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_DAY(date));
        datetime->weekday = (uint8_t)__LL_RTC_GET_WEEKDAY(date);

        result = true;
    }

    return result;
}


bool uni_hal_rtc_set(uni_hal_rtc_context_t *ctx, const uni_hal_rtc_datetime_t *datetime) {
    bool result = false;

    if (uni_hal_rtc_is_inited(ctx) && _uni_hal_rtc_datetime_is_valid(datetime)) {
        LL_RTC_DateTypeDef date = {
            .WeekDay = datetime->weekday,
            .Month = datetime->month,
            .Day = datetime->day,
            .Year = (uint8_t)(datetime->year - UNI_HAL_RTC_YEAR_BASE),
        };
        LL_RTC_TimeTypeDef time = {
            .TimeFormat = LL_RTC_TIME_FORMAT_AM_OR_24,
            .Hours = datetime->hours,
            .Minutes = datetime->minutes,
            .Seconds = datetime->seconds,
        };

        result = LL_RTC_DATE_Init(RTC, LL_RTC_FORMAT_BIN, &date) == SUCCESS;
        result = result && LL_RTC_TIME_Init(RTC, LL_RTC_FORMAT_BIN, &time) == SUCCESS;
        if (result) {
            ctx->calendar_valid = true;
        }
    }

    return result;
}


uint32_t uni_hal_rtc_backup_read(uint32_t index) {
    uint32_t result = 0U;
    if (index < UNI_HAL_RTC_BACKUP_COUNT) {
        _uni_hal_rtc_bus_clock_enable();
        result = LL_RTC_BAK_GetRegister(RTC, index);
    }
    return result;
}


bool uni_hal_rtc_backup_write(uint32_t index, uint32_t value) {
    bool result = false;
    if (index < UNI_HAL_RTC_BACKUP_COUNT) {
        _uni_hal_rtc_bus_clock_enable();
        LL_PWR_EnableBkUpAccess();
        LL_RTC_BAK_SetRegister(RTC, index, value);
        result = LL_RTC_BAK_GetRegister(RTC, index) == value;
    }
    return result;
}
