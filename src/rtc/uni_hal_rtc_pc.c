//
// Includes
//

// stdlib
#include <stddef.h>

// Uni.HAL
#include "rtc/uni_hal_rtc.h"



//
// Globals
//

static uni_hal_rtc_datetime_t g_uni_hal_rtc_datetime = {.year = 2000U, .month = 1U, .day = 1U, .weekday = 6U};
static uint32_t g_uni_hal_rtc_backup[UNI_HAL_RTC_BACKUP_COUNT] = {0U};



//
// Functions
//

// The host has no backup domain: the calendar is a value that stays as it was set, and the
// backup registers live in RAM for the lifetime of the process.

uni_hal_rcc_clksrc_e uni_hal_rtc_clock_source_get(void) {
    return UNI_HAL_RCC_CLKSRC_NONE;
}


bool uni_hal_rtc_init(uni_hal_rtc_context_t *ctx) {
    bool result = false;
    if (ctx != NULL) {
        ctx->inited = true;
        result = true;
    }
    return result;
}


bool uni_hal_rtc_is_inited(const uni_hal_rtc_context_t *ctx) {
    return ctx != NULL && ctx->inited;
}


bool uni_hal_rtc_get(const uni_hal_rtc_context_t *ctx, uni_hal_rtc_datetime_t *datetime) {
    bool result = false;
    if (uni_hal_rtc_is_inited(ctx) && datetime != NULL) {
        *datetime = g_uni_hal_rtc_datetime;
        result = true;
    }
    return result;
}


bool uni_hal_rtc_set(uni_hal_rtc_context_t *ctx, const uni_hal_rtc_datetime_t *datetime) {
    bool result = false;
    if (uni_hal_rtc_is_inited(ctx) && datetime != NULL) {
        g_uni_hal_rtc_datetime = *datetime;
        ctx->calendar_valid = true;
        result = true;
    }
    return result;
}


uint32_t uni_hal_rtc_backup_read(uint32_t index) {
    return index < UNI_HAL_RTC_BACKUP_COUNT ? g_uni_hal_rtc_backup[index] : 0U;
}


bool uni_hal_rtc_backup_write(uint32_t index, uint32_t value) {
    bool result = false;
    if (index < UNI_HAL_RTC_BACKUP_COUNT) {
        g_uni_hal_rtc_backup[index] = value;
        result = true;
    }
    return result;
}
