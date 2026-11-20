#pragma once

#include "rtc/uni_hal_rtc.h"

/** The calendar registers hold two digits of the year. */
enum { UNI_HAL_RTC_YEAR_BASE = 2000U };

static inline bool uni_hal_rtc_datetime_is_valid(const uni_hal_rtc_datetime_t *datetime) {
    static const uint8_t days_per_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (!datetime || datetime->year < UNI_HAL_RTC_YEAR_BASE || datetime->year >= UNI_HAL_RTC_YEAR_BASE + 100U ||
        datetime->month < 1U || datetime->month > 12U) {
        return false;
    }
    uint8_t last_day = days_per_month[datetime->month - 1U];
    // Within the RTC's 2000-2099 range, every fourth year is a leap year.
    if (datetime->month == 2U && datetime->year % 4U == 0U) {
        ++last_day;
    }
    return datetime->day >= 1U && datetime->day <= last_day &&
           datetime->weekday >= 1U && datetime->weekday <= 7U &&
           datetime->hours <= 23U && datetime->minutes <= 59U && datetime->seconds <= 59U;
}
