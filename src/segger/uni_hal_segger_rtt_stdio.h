#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//
// Includes
//

// stdlib
#include <stdint.h>

// uni_hal
#include "uni_common.h"
#include "io/uni_hal_io.h"
#include "tim/uni_hal_tim.h"



//
// Structs
//

/**
 * Segger IO context
 */
typedef struct {
    /**
     * IO interface
     */
    uni_hal_io_context_t *io;

    /**
     * Timer interface
     */
    uni_hal_tim_context_t *timer;

    /**
     * Segger Down Buffer Index
     */
    uint32_t buffer_index_down;

    /**
     * SEGGER up-buffer index. The polling interrupt drops bytes when the
     * channel is full and records them in io->stats.tx_dropped.
     */
    uint32_t buffer_index_up;

    /**
     * Flag which stores Segger RTT init state
     */
    bool initialized;
} uni_hal_segger_context_t;



//
// Functions
//

/**
 * Bind the IO streams to a dedicated timer and start polling RTT. The context,
 * IO and timer must remain alive while the timer runs. Index 0 uses the SEGGER
 * terminal buffers. Configure other RTT buffers before binding them and keep
 * the indices, buffers and their storage valid while active. Existing IO streams
 * are reused. Each timer interrupt reads up to the bytes pending on its down
 * channel at entry, then drains up to the bytes queued when its TX phase begins.
 * Failure releases streams created by this call and permits retry.
 */
bool uni_hal_segger_rtt_stdio_init(uni_hal_segger_context_t* ctx);

bool uni_hal_segger_rtt_stdio_is_inited(const uni_hal_segger_context_t* ctx);

#if defined(__cplusplus)
}
#endif
