#pragma once

//
// Includes
//

#include <limits.h>
#include <stdint.h>


//
// Macros
//

#define UNI_HAL_DWT_UNITS_PER_MILLISECOND (1000U)
#define UNI_HAL_DWT_UNITS_PER_SECOND_MS   (1000U)
#define UNI_HAL_DWT_UNITS_PER_SECOND_US   (1000000U)
#define UNI_HAL_DWT_UNITS_PER_SECOND_NS   (1000000000U)
#define UNI_HAL_DWT_MIN_TIMEOUT_TICK      (1U)


//
// Functions
//

static inline uint32_t uni_hal_dwt_internal_clamp_u64_to_u32(uint64_t value)
{
    return (value > (uint64_t)UINT32_MAX) ? UINT32_MAX : (uint32_t)value;
}

static inline uint32_t uni_hal_dwt_internal_convert_ticks_scaled_clamped(uint32_t tick, uint32_t clock_hz,
        uint32_t units_per_second)
{
    if (clock_hz == 0U) {
        return 0U;
    }

    // tick is in CPU cycles, clock_hz is cycles/second.
    // Convert via: time = tick * units_per_second / clock_hz.
    const uint64_t scaled = (uint64_t)tick * (uint64_t)units_per_second;
    const uint64_t result = scaled / (uint64_t)clock_hz;
    return uni_hal_dwt_internal_clamp_u64_to_u32(result);
}

static inline uint32_t uni_hal_dwt_internal_timeout_ms_to_tick(uint32_t timeout_ms, uint32_t clock_hz)
{
    const uint64_t timeout_ticks =
            ((uint64_t)timeout_ms * (uint64_t)clock_hz) / (uint64_t)UNI_HAL_DWT_UNITS_PER_SECOND_MS;

    if (timeout_ticks == 0ULL) {
        return UNI_HAL_DWT_MIN_TIMEOUT_TICK;
    }

    return uni_hal_dwt_internal_clamp_u64_to_u32(timeout_ticks);
}

static inline uint32_t uni_hal_dwt_internal_delay_us_to_tick_ceil(uint32_t us, uint32_t clock_hz)
{
    if (clock_hz == 0U) {
        return 0U;
    }

    const uint64_t numerator = ((uint64_t)us * (uint64_t)clock_hz) + ((uint64_t)UNI_HAL_DWT_UNITS_PER_SECOND_US - 1ULL);
    const uint64_t ticks = numerator / (uint64_t)UNI_HAL_DWT_UNITS_PER_SECOND_US;
    return uni_hal_dwt_internal_clamp_u64_to_u32(ticks);
}

static inline uint32_t uni_hal_dwt_internal_ms_to_us_clamped(uint32_t ms)
{
    const uint64_t us = (uint64_t)ms * (uint64_t)UNI_HAL_DWT_UNITS_PER_MILLISECOND;
    return uni_hal_dwt_internal_clamp_u64_to_u32(us);
}
