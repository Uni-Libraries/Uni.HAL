#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//
// Includes
//

// stdlib
#include <stdbool.h>
#include <stdint.h>

// Uni.HAL
#include "rcc/uni_hal_rcc_enum.h"



//
// Defines
//

/**
 * Number of backup registers
 */
#define UNI_HAL_RTC_BACKUP_COUNT (32U)



//
// Typedefs
//

/**
 * Calendar date and time
 */
typedef struct {
    /** full year, 2000..2099 */
    uint16_t year;
    /** 1..12 */
    uint8_t month;
    /** 1..31 */
    uint8_t day;
    /** 1 (Monday)..7 (Sunday) */
    uint8_t weekday;
    /** 0..23 */
    uint8_t hours;
    /** 0..59 */
    uint8_t minutes;
    /** 0..59 */
    uint8_t seconds;
} uni_hal_rtc_datetime_t;


/**
 * RTC context
 */
typedef struct {
    /**
     * Clock of the RTC, used when the RTC has none yet: UNI_HAL_RCC_CLKSRC_LSE or
     * UNI_HAL_RCC_CLKSRC_LSI. The oscillator must already be running (see the RCC driver).
     * A source selected on an earlier boot is kept, because changing it requires a reset of the
     * backup domain, which would erase the calendar and the backup registers.
     */
    uni_hal_rcc_clksrc_e clock_source;

    /**
     * Set by uni_hal_rtc_init()
     */
    bool inited;

    /**
     * The calendar was already set when uni_hal_rtc_init() ran, i.e. the time survived the reset.
     * The hardware tells this by a year other than 2000, so a calendar set to the year 2000
     * counts as not set.
     */
    bool calendar_valid;
} uni_hal_rtc_context_t;



//
// Functions
//

/**
 * Initialise the real-time clock. A calendar that is already running is left untouched.
 * Write access to the backup domain stays enabled afterwards: the RTC and the backup
 * registers need it.
 * @param ctx pointer to the RTC context
 * @return true on success
 */
bool uni_hal_rtc_init(uni_hal_rtc_context_t *ctx);

/**
 * Check that the RTC was initialised
 * @param ctx pointer to the RTC context
 * @return true when initialised
 */
bool uni_hal_rtc_is_inited(const uni_hal_rtc_context_t *ctx);

/**
 * Get the clock the RTC actually runs from
 * @return UNI_HAL_RCC_CLKSRC_LSE, UNI_HAL_RCC_CLKSRC_LSI, UNI_HAL_RCC_CLKSRC_HSE_DIV_32 or
 *         UNI_HAL_RCC_CLKSRC_NONE
 */
uni_hal_rcc_clksrc_e uni_hal_rtc_clock_source_get(void);

/**
 * Read the calendar
 * @param ctx pointer to the RTC context
 * @param datetime receives date and time
 * @return true on success
 */
bool uni_hal_rtc_get(const uni_hal_rtc_context_t *ctx, uni_hal_rtc_datetime_t *datetime);

/**
 * Set the calendar
 * @param ctx pointer to the RTC context
 * @param datetime date and time to set
 * @return false when a field is out of range or the RTC did not accept the value
 */
bool uni_hal_rtc_set(uni_hal_rtc_context_t *ctx, const uni_hal_rtc_datetime_t *datetime);

/**
 * Read a backup register. The registers keep their value across every reset as long as the
 * backup domain is powered.
 * @param index register number, 0..UNI_HAL_RTC_BACKUP_COUNT-1
 * @return register value, 0 for an invalid index
 */
uint32_t uni_hal_rtc_backup_read(uint32_t index);

/**
 * Write a backup register
 * @param index register number, 0..UNI_HAL_RTC_BACKUP_COUNT-1
 * @param value value to store
 * @return true when the register holds the value afterwards
 */
bool uni_hal_rtc_backup_write(uint32_t index, uint32_t value);

#if defined(__cplusplus)
}
#endif
