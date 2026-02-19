#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//
// Includes
//

#include <stdbool.h>
#include <stdint.h>



//
// Functions
//

/**
 * Initializes DWT counter
 * @return true on success
 */
bool uni_hal_dwt_init(void);

/**
 * Checks taht DWT was inited
 * @return true on success
 */
bool uni_hal_dwt_is_inited(void);


//
// Functions/Delay
//

/**
 * Delay execution for a specific amount of milliseconds
 * @param ms number of milliseconds to wait
 */
void uni_hal_dwt_delay_ms(uint32_t ms);

/**
 * Delay execution for a specific amount of microseconds
 * @param us number of microseconds to wait
 */
void uni_hal_dwt_delay_us(uint32_t us);


//
// Functions/Get
//

/**
 * Get current DWT counter value in milliseconds
 * @return DWT counter value in milliseconds
 */
uint32_t uni_hal_dwt_get_ms(void);

/**
 * Get current DWT counter value in microseconds
 * @return DWT counter value in microseconds
 */
uint32_t uni_hal_dwt_get_us(void);

/**
 * Get current DWT counter value in counter ticks
 * @return DWT counter value in ticks
 */
uint32_t uni_hal_dwt_get_tick(void);



//
// Functions/Compare
//

/**
 * Compares two timestamps (ticks) taking 32-bit counter overflow into account.
 *
 * Semantics: returns elapsed ticks from timestamp_1 to timestamp_2:
 *   delta = (timestamp_2 - timestamp_1) mod 2^32
 *
 * This is suitable for timeout/duration checks like:
 *   if (uni_hal_dwt_compare(start, now) >= timeout_ticks) { ... }
 */
uint32_t uni_hal_dwt_compare(uint32_t timestamp_1, uint32_t timestamp_2);



//
// Functions/Convert
//

/**
 * Converts counter ticks to milliseconds, clamped to UINT32_MAX.
 */
uint32_t uni_hal_dwt_convert_ms(uint32_t tick);

/**
 * Converts counter ticks to nanoseconds, clamped to UINT32_MAX.
 */
uint32_t uni_hal_dwt_convert_ns(uint32_t tick);

/**
 * Converts counter ticks to microseconds, clamped to UINT32_MAX.
 */
uint32_t uni_hal_dwt_convert_us(uint32_t tick);

/**
 * Converts timeout in milliseconds to DWT counter ticks, clamped to UINT32_MAX.
 *
 * The result is guaranteed to be at least 1 tick.
 */
uint32_t uni_hal_dwt_timeout_ms_to_tick(uint32_t timeout_ms);


#if defined(__cplusplus)
}
#endif
