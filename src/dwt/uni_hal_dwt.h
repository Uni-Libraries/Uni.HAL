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

/**
 * Delay execution for a specific amount of milliseconds
 * @param ms number of milliseconds to wait
 */
void uni_hal_dwt_delay_ms(uint32_t ms);

/**
 * Delay execution for a specific amount of microseconds
 * @param ms number of microseconds to wait
 */
void uni_hal_dwt_delay_us(uint32_t us);


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


#if defined(__cplusplus)
}
#endif
