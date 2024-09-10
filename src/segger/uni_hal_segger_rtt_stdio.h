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
     * Segger Up Buffer Index
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

bool uni_hal_segger_rtt_stdio_init(uni_hal_segger_context_t* ctx);

bool uni_hal_segger_rtt_stdio_is_inited(const uni_hal_segger_context_t* ctx);

#if defined(__cplusplus)
}
#endif
