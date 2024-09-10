#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//
// Includes
//

// stdlib
#include <stdbool.h>



//
// Typedefs
//

/**
 * PWR context
 */
typedef struct {
    /**
     * Enable battery charging
     */
    bool battery_charging;

    /**
     * Inited flag
     */
     bool inited;
} uni_hal_pwr_context_t;



//
// Functions
//

/**
 * Reset the MCU
 */
void uni_hal_pwr_reset(void);

/**
 * Initialize MCU power subsystem
 * @param ctx PWR context
 * @return true on success
 */
bool uni_hal_pwr_init(void);

/**
 * Checks that PWR was properly inited
 * @param ctx PWR context
 * @return true on success
 */
bool uni_hal_pwr_is_inited(void);

/**
 * Is battery charging enabled
 * @param ctx PWR context
 * @return true on success
 */
bool uni_hal_pwr_is_battery_charging(void);


bool uni_hal_pwr_set_battery_charging(bool value);


#if defined(__cplusplus)
}
#endif
