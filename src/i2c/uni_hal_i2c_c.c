//
// Includes
//

// stdlib
#include <stddef.h>

// uni_hal
#include "i2c/uni_hal_i2c.h"



//
// Functions
//

bool uni_hal_i2c_is_inited(uni_hal_i2c_context_t *ctx) {
    bool result = false;
    if (ctx != NULL) {
        result = ctx->state.initialized;
    }
    return result;
}


bool uni_hal_i2c_reset(uni_hal_i2c_context_t *ctx) {
    bool result = false;
    if (ctx != NULL) {
        uni_hal_i2c_deinit(ctx);
        result = uni_hal_i2c_init(ctx);
    }

    return result;
}


//
// Timing
//

/**
 * Bus timing limits of one I2C speed, in nanoseconds (UM10204, table 10)
 */
typedef struct {
    uint32_t freq_hz;
    uint32_t t_low_min;
    uint32_t t_high_min;
    uint32_t t_su_dat_min;
    uint32_t t_rise_max;
} uni_hal_i2c_timing_spec_t;

enum {
    /** shortest delay of the analog filter, ns */
    UNI_HAL_I2C_TIMING_AF_MIN_NS = 50U,
    /** longest delay of the analog filter, ns */
    UNI_HAL_I2C_TIMING_AF_MAX_NS = 260U,
};

static uint64_t _uni_hal_i2c_div_ceil(uint64_t value, uint64_t divider) {
    return (value + divider - 1U) / divider;
}

/** Convert nanoseconds to I2C kernel clock periods, rounded up */
static uint64_t _uni_hal_i2c_ns_to_ticks(uint32_t ns, uint32_t clock_hz) {
    return _uni_hal_i2c_div_ceil((uint64_t)ns * clock_hz, 1'000'000'000ULL);
}

uint32_t uni_hal_i2c_timing_calc(uint32_t clock_hz, uni_hal_i2c_speed_e speed) {
    uni_hal_i2c_timing_spec_t spec;
    switch (speed) {
    case UNI_HAL_I2C_SPEED_100KHZ:
        spec = (uni_hal_i2c_timing_spec_t){100'000U, 4700U, 4000U, 250U, 1000U};
        break;
    case UNI_HAL_I2C_SPEED_400KHZ:
        spec = (uni_hal_i2c_timing_spec_t){400'000U, 1300U, 600U, 100U, 300U};
        break;
    case UNI_HAL_I2C_SPEED_1MHZ:
        spec = (uni_hal_i2c_timing_spec_t){1'000'000U, 500U, 260U, 50U, 120U};
        break;
    default:
        return 0U;
    }

    if (clock_hz == 0U) {
        return 0U;
    }

    // The peripheral needs a kernel clock that is fast against the bus (RM0433 / RM0351):
    // tI2CCLK < (tLOW - tfilters) / 4 and tI2CCLK < tHIGH
    uint64_t const clock_period_ps = 1'000'000'000'000ULL / clock_hz;
    if ((clock_period_ps * 4U) >= ((uint64_t)(spec.t_low_min - UNI_HAL_I2C_TIMING_AF_MAX_NS) * 1000U) ||
        clock_period_ps >= ((uint64_t)spec.t_high_min * 1000U)) {
        return 0U;
    }

    // SCL period in kernel clock periods. Each edge is resynchronised, which adds at least the
    // analog filter delay and two clock periods. Taking that minimum here means the bus never
    // runs faster than asked for; real rise times only make it slower.
    uint64_t const period_ticks = _uni_hal_i2c_div_ceil(clock_hz, spec.freq_hz);
    uint64_t const sync_ticks = 2U * ((((uint64_t)UNI_HAL_I2C_TIMING_AF_MIN_NS * clock_hz) / 1'000'000'000ULL) + 2U);
    uint64_t const counted_ticks = (period_ticks > sync_ticks) ? (period_ticks - sync_ticks) : 0U;

    // smallest prescaler first: it gives the finest resolution
    for (uint32_t presc = 0U; presc < 16U; presc++) {
        uint64_t const div = (uint64_t)presc + 1U;

        // SCLDEL: data set-up time, counted from the end of the rising edge
        uint64_t const scldel =
                _uni_hal_i2c_div_ceil(_uni_hal_i2c_ns_to_ticks(spec.t_rise_max + spec.t_su_dat_min, clock_hz), div);
        if (scldel > 16U) {
            continue;
        }

        uint64_t const low_min = _uni_hal_i2c_div_ceil(_uni_hal_i2c_ns_to_ticks(spec.t_low_min, clock_hz), div);
        uint64_t const high_min = _uni_hal_i2c_div_ceil(_uni_hal_i2c_ns_to_ticks(spec.t_high_min, clock_hz), div);

        // split the period between low and high in the proportion of their minimums
        uint64_t total = _uni_hal_i2c_div_ceil(counted_ticks, div);
        if (total < (low_min + high_min)) {
            total = low_min + high_min;
        }
        uint64_t low = _uni_hal_i2c_div_ceil(total * spec.t_low_min, (uint64_t)spec.t_low_min + spec.t_high_min);
        if (low < low_min) {
            low = low_min;
        }
        uint64_t high = (total > low) ? (total - low) : 0U;
        if (high < high_min) {
            high = high_min;
        }

        if (low > 256U || high > 256U) {
            continue;
        }

        uint64_t const scldel_reg = (scldel > 0U) ? (scldel - 1U) : 0U;
        // SDADEL stays 0: the minimum data hold time of the bus is 0
        return (presc << 28U) | ((uint32_t)scldel_reg << 20U) | ((uint32_t)(high - 1U) << 8U) | (uint32_t)(low - 1U);
    }

    return 0U;
}
