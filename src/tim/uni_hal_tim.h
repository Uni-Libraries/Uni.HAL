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

// uni_hal
#include "core/uni_hal_core.h"
#include "rcc/uni_hal_rcc.h"



//
// Typedefs
//

typedef bool (*uni_hal_tim_callback_fn)(void *ctx_timer, void *ctx_fn);


//
// Typedefs
//

/**
 * TIM config
 */
typedef struct {
    /**
     * TIM instance
     */
    uni_hal_core_periph_e instance;

    /**
     * TIM callback
     */
    uni_hal_tim_callback_fn callback;

    /**
     * Presccaler
     */
     uint32_t prescaler;

     /**
      * Period
      */
      uint32_t period;
} uni_hal_tim_config_t;

/**
 * TIM status
 */
typedef struct {
    bool inited;
} uni_hal_tim_status_t;

/**
 * TIM context
 */
typedef struct {
    /**
     * Config
     */
    uni_hal_tim_config_t config;

    /**
     * Status
     */
    uni_hal_tim_status_t status;
} uni_hal_tim_context_t;


//
// Functions
//

bool uni_hal_tim_init(uni_hal_tim_context_t *ctx);

bool uni_hal_tim_is_inited(const uni_hal_tim_context_t *ctx);

bool uni_hal_tim_register_callback(uni_hal_tim_context_t *ctx, uni_hal_tim_callback_fn callback, void *callback_ctx);

bool uni_hal_tim_start(uni_hal_tim_context_t *ctx);

bool uni_hal_tim_stop(uni_hal_tim_context_t *ctx);

bool uni_hal_tim_period_elapsed(uni_hal_core_periph_e periph);

#if defined(__cplusplus)
}
#endif
