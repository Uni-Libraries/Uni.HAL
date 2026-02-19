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



//
// Typedefs
//

/**
 * Runtime state of the PWR subsystem.
 */
typedef struct {
    /**
     * Battery charging feature state.
     *
     * `true` when VBAT charging path is enabled via
     * [`uni_hal_pwr_set_battery_charging()`](src_unihal/src/pwr/uni_hal_pwr.h:98).
     */
    bool battery_charging;

    /**
     * Initialization status of the PWR subsystem.
     *
     * Set by [`uni_hal_pwr_init()`](src_unihal/src/pwr/uni_hal_pwr.h:76).
     */
     bool inited;

    /**
     * Last selected run-mode regulator voltage scaling.
     *
     * Value is one of `LL_PWR_REGU_VOLTAGE_SCALE_x` sampled after
     * successful initialization.
     */
    uint32_t voltage_scale;

    /**
     * Last sampled PWR status snapshot for diagnostics.
     *
     * Captures `PWR->CSR1` after init attempt (success or failure) and can be
     * used to inspect boot-time power state.
     */
    uint32_t last_state;
} uni_hal_pwr_context_t;



//
// Functions
//

/**
 * Perform MCU reset through the active platform implementation.
 */
void uni_hal_pwr_reset(void);

/**
 * Initialize MCU power subsystem.
 *
 * Configures required power/supply/voltage scaling sequence for the target.
 *
 * @retval true  Initialization completed successfully.
 * @retval false Initialization failed.
 */
bool uni_hal_pwr_init(void);

/**
 * Check whether PWR subsystem is initialized.
 *
 * @retval true  PWR is initialized.
 * @retval false PWR is not initialized.
 */
bool uni_hal_pwr_is_inited(void);

/**
 * Check whether VBAT battery charging is enabled.
 *
 * @retval true  Charging is enabled.
 * @retval false Charging is disabled or PWR is not initialized.
 */
bool uni_hal_pwr_is_battery_charging(void);


/**
 * Enable or disable VBAT battery charging path.
 *
 * @param value Desired state: `true` to enable, `false` to disable.
 * @retval true Request was applied.
 */
bool uni_hal_pwr_set_battery_charging(bool value);


#if defined(__cplusplus)
}
#endif
