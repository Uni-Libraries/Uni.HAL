//
// Includes
//

// stdlib
#include <stddef.h>

#include "uni_hal_target.h"

// ST
#if defined(UNI_HAL_TARGET_MCU_STM32L496)
    #include <stm32l4xx_ll_bus.h>
    #include <stm32l4xx_ll_pwr.h>
    #include <stm32l4xx_ll_rcc.h>
    #include <stm32l4xx_ll_rtc.h>
#elif defined(UNI_HAL_TARGET_STM32H7)
    #include <stm32h7xx_ll_bus.h>
    #include <stm32h7xx_ll_pwr.h>
    #include <stm32h7xx_ll_rcc.h>
    #include <stm32h7xx_ll_rtc.h>
#else
    #error "unknown MCU"
#endif

// Uni.HAL
#include "core/uni_hal_core.h"
#include "rtc/uni_hal_rtc_calendar.h"
#include "systick/uni_hal_systick_stm32_wait.h"



//
// Defines
//

enum {
    /** 1 Hz from the 32.768 kHz crystal: 32768 / (127 + 1) / (255 + 1) */
    UNI_HAL_RTC_LSE_PREDIV_A = 127U,
    UNI_HAL_RTC_LSE_PREDIV_S = 255U,

    /** 1 Hz from the 32 kHz internal oscillator: 32000 / (127 + 1) / (249 + 1) */
    UNI_HAL_RTC_LSI_PREDIV_A = 127U,
    UNI_HAL_RTC_LSI_PREDIV_S = 249U,

    /** initialization and shadow-register acknowledgements, as in the ST LL driver */
    UNI_HAL_RTC_ACK_TIMEOUT_MS = 1000U,

    /** longest wait for LSE to start again after a reset of the backup domain */
    UNI_HAL_RTC_LSE_TIMEOUT_MS = 5000U,
};



//
// Private
//

static void _uni_hal_rtc_bus_clock_enable(void) {
    uint32_t const mask = uni_hal_core_irq_pause();
#if defined(UNI_HAL_TARGET_MCU_STM32L496)
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR | LL_APB1_GRP1_PERIPH_RTCAPB);
#else
    LL_APB4_GRP1_EnableClock(LL_APB4_GRP1_PERIPH_RTCAPB);
#endif
    uni_hal_core_irq_resume(mask);
}



/**
 * Reset the backup domain, which is the only way to make the clock of the RTC selectable again
 */
static void _uni_hal_rtc_backup_reset(void) {
    // The reset clears the whole control register of the backup domain, the LSE settings among
    // it. They are put back, without the clock selection of the RTC, so that LSE keeps its
    // bypass and drive settings and starts again when it was on.
    uint32_t const bdcr = READ_REG(RCC->BDCR) & ~(RCC_BDCR_RTCSEL | RCC_BDCR_RTCEN);
    LL_RCC_ForceBackupDomainReset();
    LL_RCC_ReleaseBackupDomainReset();
    WRITE_REG(RCC->BDCR, bdcr);

    if ((bdcr & RCC_BDCR_LSEON) != 0U) {
        // The caller verifies readiness of the requested clock, which may be LSI.
        (void)uni_hal_stm32_wait_bits(&RCC->BDCR, RCC_BDCR_LSERDY, RCC_BDCR_LSERDY, UNI_HAL_RTC_LSE_TIMEOUT_MS);
    }
}


static bool _uni_hal_rtc_enter_init_mode(void) {
    if (LL_RTC_IsActiveFlag_INIT(RTC)) {
        return true;
    }
    LL_RTC_EnableInitMode(RTC);
    return uni_hal_stm32_wait_bits(&RTC->ISR, RTC_ISR_INITF, RTC_ISR_INITF, UNI_HAL_RTC_ACK_TIMEOUT_MS);
}

/**
 * Check whether the RTC has to be moved to the clock source of the context
 */
static bool _uni_hal_rtc_clock_source_differs(const uni_hal_rtc_context_t *ctx) {
    uint32_t const current = LL_RCC_GetRTCClockSource();
    bool result = false;
    if (current != LL_RCC_RTC_CLKSOURCE_NONE) {
        // only for a source that can take over
        if (ctx->clock_source == UNI_HAL_RCC_CLKSRC_LSE) {
            result = current != LL_RCC_RTC_CLKSOURCE_LSE && LL_RCC_LSE_IsReady() != 0U;
        }
        else if (ctx->clock_source == UNI_HAL_RCC_CLKSRC_LSI) {
            result = current != LL_RCC_RTC_CLKSOURCE_LSI && LL_RCC_LSI_IsReady() != 0U;
        }
        else {
            // not a source this driver selects
        }
    }
    return result;
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

    if (ctx != nullptr) {
        _uni_hal_rtc_bus_clock_enable();
        LL_PWR_EnableBkUpAccess();

        // The clock selection is write-once until the backup domain is reset. Select a source
        // only when there is none, so that a running calendar is never disturbed, unless the
        // context asks for the change and accepts the loss.
        if (ctx->clock_source_change && _uni_hal_rtc_clock_source_differs(ctx)) {
            _uni_hal_rtc_backup_reset();
        }

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
                LL_RTC_DisableWriteProtection(RTC);
                result = _uni_hal_rtc_enter_init_mode();
                if (result) {
                    LL_RTC_SetHourFormat(RTC, LL_RTC_HOURFORMAT_24HOUR);
                    LL_RTC_SetSynchPrescaler(RTC, from_lsi ? UNI_HAL_RTC_LSI_PREDIV_S : UNI_HAL_RTC_LSE_PREDIV_S);
                    LL_RTC_SetAsynchPrescaler(RTC, from_lsi ? UNI_HAL_RTC_LSI_PREDIV_A : UNI_HAL_RTC_LSE_PREDIV_A);
                }
                LL_RTC_DisableInitMode(RTC);
                LL_RTC_EnableWriteProtection(RTC);
            }
        }

        ctx->inited = result;
    }

    return result;
}


bool uni_hal_rtc_is_inited(const uni_hal_rtc_context_t *ctx) {
    return ctx != nullptr && ctx->inited;
}


typedef struct {
    uint32_t subsecond, time, date;
} rtc_snapshot_t;

static rtc_snapshot_t read_snapshot(void) {
    rtc_snapshot_t sample;
    sample.subsecond = LL_RTC_TIME_GetSubSecond(RTC);
    sample.time = READ_REG(RTC->TR);
    sample.date = LL_RTC_DATE_Get(RTC); // Release the shadow latch on every path.
    return sample;
}

static bool read_consistent_snapshot(rtc_snapshot_t *sample) {
    // System reset clears the shadow registers even when the calendar was retained.
    if (!LL_RTC_IsShadowRegBypassEnabled(RTC) && !LL_RTC_IsActiveFlag_RS(RTC)) {
        return false;
    }
    // In bypass mode registers can change independently. A shadow lock can also be
    // missed at an update (STM32H743/H753 ES0392). Compare two complete calendar reads.
    rtc_snapshot_t previous = read_snapshot();
    for (unsigned retry = 0U; retry < 3U; ++retry) {
        *sample = read_snapshot();
        if (sample->subsecond == previous.subsecond && sample->time == previous.time && sample->date == previous.date) {
            return true;
        }
        previous = *sample;
    }
    return false;
}


bool uni_hal_rtc_get(const uni_hal_rtc_context_t *ctx, uni_hal_rtc_datetime_t *datetime) {
    bool result = false;

    rtc_snapshot_t sample;
    if (uni_hal_rtc_is_inited(ctx) && datetime != nullptr && read_consistent_snapshot(&sample)) {
        // the sub-second counter runs down from the synchronous prescaler to 0 within a second
        uint32_t const prescaler = LL_RTC_GetSynchPrescaler(RTC);
        uint32_t milliseconds = 0U;
        if (sample.subsecond <= prescaler) {
            milliseconds = (1000U * (prescaler - sample.subsecond)) / (prescaler + 1U);
        }
        datetime->milliseconds = (uint16_t)((milliseconds < 1000U) ? milliseconds : 999U);

        uint32_t const hour_bcd = (sample.time & (RTC_TR_HT | RTC_TR_HU)) >> RTC_TR_HU_Pos;
        uint8_t hour = (uint8_t)__LL_RTC_CONVERT_BCD2BIN(hour_bcd);
        if (LL_RTC_GetHourFormat(RTC) == LL_RTC_HOURFORMAT_AMPM) {
            hour = (uint8_t)((hour % 12U) + ((sample.time & RTC_TR_PM) ? 12U : 0U));
        }
        datetime->hours = hour;
        datetime->minutes = (uint8_t)__LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_MINUTE(sample.time));
        datetime->seconds = (uint8_t)__LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_SECOND(sample.time));
        datetime->year = (uint16_t)(UNI_HAL_RTC_YEAR_BASE + __LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_YEAR(sample.date)));
        datetime->month = (uint8_t)__LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_MONTH(sample.date));
        datetime->day = (uint8_t)__LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_DAY(sample.date));
        datetime->weekday = (uint8_t)__LL_RTC_GET_WEEKDAY(sample.date);

        result = true;
    }

    return result;
}


bool uni_hal_rtc_set(uni_hal_rtc_context_t *ctx, const uni_hal_rtc_datetime_t *datetime) {
    bool result = false;

    if (uni_hal_rtc_is_inited(ctx) && uni_hal_rtc_datetime_is_valid(datetime)) {
        uint32_t format = LL_RTC_TIME_FORMAT_AM_OR_24;
        uint8_t hour = datetime->hours;
        if (LL_RTC_GetHourFormat(RTC) == LL_RTC_HOURFORMAT_AMPM) {
            format = hour >= 12U ? LL_RTC_TIME_FORMAT_PM : LL_RTC_TIME_FORMAT_AM_OR_24;
            hour %= 12U;
            if (hour == 0U) {
                hour = 12U;
            }
        }

        // Keep the calendar stopped until it holds the complete date and time.
        LL_RTC_DisableWriteProtection(RTC);
        result = _uni_hal_rtc_enter_init_mode();
        if (result) {
            LL_RTC_DATE_Config(RTC, datetime->weekday, __LL_RTC_CONVERT_BIN2BCD(datetime->day),
                               __LL_RTC_CONVERT_BIN2BCD(datetime->month),
                               __LL_RTC_CONVERT_BIN2BCD(datetime->year - UNI_HAL_RTC_YEAR_BASE));
            LL_RTC_TIME_Config(RTC, format, __LL_RTC_CONVERT_BIN2BCD(hour),
                               __LL_RTC_CONVERT_BIN2BCD(datetime->minutes), __LL_RTC_CONVERT_BIN2BCD(datetime->seconds));
        }
        // Also withdraw the INIT request if the acknowledgement timed out.
        LL_RTC_DisableInitMode(RTC);
        if (result && !LL_RTC_IsShadowRegBypassEnabled(RTC)) {
            LL_RTC_ClearFlag_RS(RTC);
            result = uni_hal_stm32_wait_bits(&RTC->ISR, RTC_ISR_RSF, 0U, UNI_HAL_RTC_ACK_TIMEOUT_MS) &&
                     uni_hal_stm32_wait_bits(&RTC->ISR, RTC_ISR_RSF, RTC_ISR_RSF, UNI_HAL_RTC_ACK_TIMEOUT_MS);
        }
        LL_RTC_EnableWriteProtection(RTC);
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
