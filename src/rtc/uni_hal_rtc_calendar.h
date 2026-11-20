#pragma once

#include "rtc/uni_hal_rtc.h"

/** The calendar registers hold two digits of the year. */
enum { UNI_HAL_RTC_YEAR_BASE = 2000U };

static inline bool uni_hal_rtc_datetime_is_valid(const uni_hal_rtc_datetime_t *datetime) {
    return datetime &&
           datetime->year >= UNI_HAL_RTC_YEAR_BASE && datetime->year < (UNI_HAL_RTC_YEAR_BASE + 100U) &&
           datetime->month >= 1U && datetime->month <= 12U &&
           datetime->day >= 1U && datetime->day <= 31U &&
           datetime->weekday >= 1U && datetime->weekday <= 7U &&
           datetime->hours <= 23U && datetime->minutes <= 59U && datetime->seconds <= 59U;
}
