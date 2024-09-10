#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//
// Defines
//

// stdlib
#include <stdbool.h>
#include <stdint.h>

// uni_hal
#include "core/uni_hal_core.h"



//
// Typedefs
//

/**
 * IWDG context structure
 */
typedef struct {
    /**
     * Watchdog Instance
     */
    uni_hal_core_periph_e instance;
    
    /**
     * Watchdog prescaler
     * possible values: 4, 8, 16, 32, 64, 128, 256
     */
    uint32_t watchdog_prescaler;

    /**
     * Watchdog counter
     * Possible values: 0-4095
     */
    uint32_t watchdog_counter;

    /**
     * Inited status
     */
    bool inited;
} uni_hal_iwdg_context_t;


//
// Functions
//

/**
 * Initializes Watchdog
 * @param ctx pointer to the watchdog context
 * @return true on success
 */
bool uni_hal_iwdg_init(uni_hal_iwdg_context_t *ctx);

/**
 * Deinitialize IWDG
 * @param ctx pointer to the IWDG context
 * @return true on success
 */
bool uni_hal_iwdg_deinit(uni_hal_iwdg_context_t *ctx);

/**
 * Check that IWDG was properly inited
 * @param ctx IWDG context
 * @return true on success
 */
bool uni_hal_iwdg_is_inited(const uni_hal_iwdg_context_t* ctx);

/**
 * Realods Watchdog
 * @param ctx pointer to the watchdog context
 * @return true on success
 */
bool uni_hal_iwdg_reload(uni_hal_iwdg_context_t *ctx);

#if defined(__cplusplus)
}
#endif
