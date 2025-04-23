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
#include "gpio/uni_hal_gpio.h"
#include "rcc/uni_hal_rcc.h"



//
// Defines
//

#define UNI_HAL_TIM_CHANNEL_MAXCOUNT 6U



//
// Typedefs
//

typedef bool (*uni_hal_tim_callback_fn)(void *ctx_timer, void *ctx_fn);

typedef enum
{
    UNI_HAL_TIM_POLARITY_RISING,
    UNI_HAL_TIM_POLARITY_FALLING,
    UNI_HAL_TIM_POLARITY_BOTH,
} uni_hal_tim_polarity_e;

typedef enum
{
    UNI_HAL_TIM_TYPE_INPUTCAPTURE,
    UNI_HAL_TIM_TYPE_OUTPUTCOMPARE,
} uni_hal_tim_type_e;

typedef enum
{
    UNI_HAL_TIM_CHANNEL_1 = 0,
    UNI_HAL_TIM_CHANNEL_2 = 1,
    UNI_HAL_TIM_CHANNEL_3 = 2,
    UNI_HAL_TIM_CHANNEL_4 = 3,
    UNI_HAL_TIM_CHANNEL_5 = 4,
    UNI_HAL_TIM_CHANNEL_6 = 5,
} uni_hal_tim_channel_num_e;

typedef struct
{
    uni_hal_tim_channel_num_e channel_number;

    uni_hal_tim_type_e type;

    uni_hal_tim_polarity_e polarity;

    uni_hal_gpio_pin_context_t* gpio;
} uni_hal_tim_channel_t;

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
      * Reload value
      */
    uint32_t reload_value;

    /**
    * Channels
    */
    uni_hal_tim_channel_t** channel;

    /**
     * Channel count
     */
    size_t channel_count;
} uni_hal_tim_config_t;

/**
 * TIM status
 */
typedef struct {
    bool inited;
    uint32_t chan_cnt[UNI_HAL_TIM_CHANNEL_MAXCOUNT];
    uint32_t chan_ms[UNI_HAL_TIM_CHANNEL_MAXCOUNT];
    uint16_t chan_val[UNI_HAL_TIM_CHANNEL_MAXCOUNT];
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

bool uni_hal_tim_clear(uni_hal_tim_context_t* ctx);

bool uni_hal_tim_start(uni_hal_tim_context_t *ctx);

bool uni_hal_tim_stop(uni_hal_tim_context_t *ctx);

bool uni_hal_tim_set_arrpreload(uni_hal_tim_context_t * ctx, bool val);

bool uni_hal_tim_set_dmarequest(uni_hal_tim_context_t * ctx, bool val);

bool uni_hal_tim_period_elapsed(uni_hal_core_periph_e periph);

uint32_t uni_hal_tim_get_period_us(uni_hal_tim_context_t *ctx);

uint32_t uni_hal_tim_get_chan_freq(uni_hal_tim_context_t *ctx, uni_hal_tim_channel_num_e chan);

uint32_t uni_hal_tim_get_chan_age(uni_hal_tim_context_t *ctx, uni_hal_tim_channel_num_e chan);

#if defined(__cplusplus)
}
#endif
