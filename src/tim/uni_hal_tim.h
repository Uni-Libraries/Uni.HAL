#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//
// Includes
//

// stdlib
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// uni_hal
#include "core/uni_hal_core.h"
#include "gpio/uni_hal_gpio.h"
#include "rcc/uni_hal_rcc.h"



//
// Defines
//

/** Maximum number of channels tracked by a timer context. */
#define UNI_HAL_TIM_CHANNEL_MAXCOUNT 6U



//
// Typedefs
//

typedef bool (*uni_hal_tim_callback_fn)(void *ctx_timer, void *ctx_fn);



//
// Enums
//

/** Input capture edge polarity. */
typedef enum
{
    UNI_HAL_TIM_POLARITY_RISING,
    UNI_HAL_TIM_POLARITY_FALLING,
    UNI_HAL_TIM_POLARITY_BOTH,
} uni_hal_tim_polarity_e;

/** Timer channel operating mode. */
typedef enum
{
    UNI_HAL_TIM_TYPE_INPUTCAPTURE,
    UNI_HAL_TIM_TYPE_OUTPUTCOMPARE,
} uni_hal_tim_type_e;

/** Logical timer channel number. */
typedef enum
{
    UNI_HAL_TIM_CHANNEL_1 = 0,
    UNI_HAL_TIM_CHANNEL_2 = 1,
    UNI_HAL_TIM_CHANNEL_3 = 2,
    UNI_HAL_TIM_CHANNEL_4 = 3,
    UNI_HAL_TIM_CHANNEL_5 = 4,
    UNI_HAL_TIM_CHANNEL_6 = 5,
} uni_hal_tim_channel_num_e;


//
// Structs
//

/**
 * TIM channel status
 */
typedef struct {
    /** Last captured counter value from CCR. */
    uint32_t counter;

    /** Overflow counter value at the last capture. */
    uint32_t overflow_count;

    /** Captured period in timer ticks. */
    uint32_t period;

    /** System tick in milliseconds when the channel was last captured. */
    uint32_t timestamp;

    /** Channel was seen at least one. */
    bool seen;

    /** Channel contains valid data. */
    bool valid;
} uni_hal_tim_channel_state_t;

/** Timer channel configuration. */
typedef struct
{
    /** Logical channel number. */
    uni_hal_tim_channel_num_e channel_number;

    /** Channel operating mode. */
    uni_hal_tim_type_e type;

    /** Input capture polarity. */
    uni_hal_tim_polarity_e polarity;

    /** Optional GPIO pin used by the channel. */
    uni_hal_gpio_pin_context_t* gpio;

    /** Raw 4-bit STM32 TIM ICxF digital filter value. */
    uint32_t filter;
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
     * Timer prescaler register value.
     */
     uint32_t prescaler;

     /**
      * Timer auto-reload register value.
      */
    uint32_t reload_value;

    /**
    * Array of channel configuration pointers.
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
    /** True after successful initialization. */
    bool inited;

    /** Number of timer update overflows since start/clear. */
    uint32_t overflow_count;

    /**
     * Channel status
     */
    uni_hal_tim_channel_state_t chan[UNI_HAL_TIM_CHANNEL_MAXCOUNT];
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

/**
 * Initialize a timer context and its configured channels.
 *
 * @param ctx Timer context to initialize.
 * @return true on success, false otherwise.
 */
bool uni_hal_tim_init(uni_hal_tim_context_t *ctx);

/**
 * Check whether a timer context was initialized successfully.
 *
 * @param ctx Timer context.
 * @return true if the context is initialized.
 */
bool uni_hal_tim_is_inited(const uni_hal_tim_context_t *ctx);

/**
 * Register a timer update callback.
 *
 * @param ctx Timer context.
 * @param callback Callback invoked from the timer interrupt path.
 * @param callback_ctx User context passed to the callback.
 * @return true if the callback was registered.
 */
bool uni_hal_tim_register_callback(uni_hal_tim_context_t *ctx, uni_hal_tim_callback_fn callback, void *callback_ctx);

/**
 * Reset the timer counter to zero.
 *
 * @param ctx Timer context.
 * @return true on success, false otherwise.
 */
bool uni_hal_tim_clear(uni_hal_tim_context_t* ctx);

/**
 * Start the timer and enable configured channel interrupts.
 *
 * @param ctx Timer context.
 * @return true on success, false otherwise.
 */
bool uni_hal_tim_start(uni_hal_tim_context_t *ctx);

/**
 * Stop the timer and disable update interrupts.
 *
 * @param ctx Timer context.
 * @return true on success, false otherwise.
 */
bool uni_hal_tim_stop(uni_hal_tim_context_t *ctx);

/**
 * Enable or disable auto-reload preload.
 *
 * @param ctx Timer context.
 * @param val true to enable preload, false to disable it.
 * @return true on success, false otherwise.
 */
bool uni_hal_tim_set_arrpreload(uni_hal_tim_context_t * ctx, bool val);

/**
 * Enable or disable update DMA requests.
 *
 * @param ctx Timer context.
 * @param val true to enable DMA requests, false to disable them.
 * @return true on success, false otherwise.
 */
bool uni_hal_tim_set_dmarequest(uni_hal_tim_context_t * ctx, bool val);

/**
 * Handle a timer update interrupt.
 *
 * @param periph Timer peripheral identifier.
 * @return true if the callback requested a context switch.
 */
bool uni_hal_tim_period_elapsed(uni_hal_core_periph_e periph);


bool uni_hal_tim_is_channel_valid(uni_hal_tim_context_t *ctx, uni_hal_tim_channel_num_e channel);


//
// get_tick_period
//

/**
 * Get one timer tick period in nanoseconds.
 *
 * @param ctx Timer context.
 * @return Tick period in nanoseconds, or 0 if unavailable.
 */
uint32_t uni_hal_tim_get_tick_period_ns(uni_hal_tim_context_t *ctx);

/**
 * Get one timer tick period in microseconds.
 *
 * @param ctx Timer context.
 * @return Tick period in microseconds, or 0 if unavailable.
 */
uint32_t uni_hal_tim_get_tick_period_us(uni_hal_tim_context_t *ctx);

//
// chan
//

bool uni_hal_tim_get_chan_state(uni_hal_tim_context_t *ctx, uni_hal_tim_channel_num_e channel, uni_hal_tim_channel_state_t *out);

//
// get_chan_period
//

/**
 * Get the last captured channel period in nanoseconds.
 *
 * @param ctx Timer context.
 * @param chan_state Channel state.
 * @return Channel period in nanoseconds, or UINT64_MAX if unavailable.
 */
uint64_t uni_hal_tim_get_chan_period_ns(uni_hal_tim_context_t *ctx, uni_hal_tim_channel_state_t* chan_state);

/**
 * Get the last captured channel period in microseconds.
 *
 * @param ctx Timer context.
 * @param chan_state Channel state.
 * @return Channel period in microseconds, or UINT64_MAX if unavailable.
 */
uint64_t uni_hal_tim_get_chan_period_us(uni_hal_tim_context_t *ctx, uni_hal_tim_channel_state_t* chan_state);

//
// get_chan_freq
//

/**
 * Get the last captured channel frequency in milli-Hz.
 *
 * @param ctx Timer context.
 * @param chan_state Channel state.
 * @return Channel frequency in milli-Hz, or UINT64_MAX if unavailable.
 */
uint64_t uni_hal_tim_get_chan_freq_mhz(uni_hal_tim_context_t *ctx, uni_hal_tim_channel_state_t* chan_state);

/**
 * Get the last captured channel frequency in Hz.
 *
 * @param ctx Timer context.
 * @param chan_state Channel state.
 * @return Channel frequency in Hz, or UINT64_MAX if unavailable.
 */
uint64_t uni_hal_tim_get_chan_freq_hz(uni_hal_tim_context_t *ctx, uni_hal_tim_channel_state_t* chan_state);

/**
 * Get the age of the last channel capture.
 *
 * @param ctx Timer context.
 * @param chan_state Channel state.
 * @return Age in milliseconds, or UINT32_MAX if unavailable.
 */
uint32_t uni_hal_tim_get_chan_age(uni_hal_tim_context_t *ctx, uni_hal_tim_channel_state_t* chan_state);

#if defined(__cplusplus)
}
#endif
