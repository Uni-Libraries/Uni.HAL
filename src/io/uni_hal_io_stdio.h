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
#include "uni_hal_io.h"


//
// Typedefs
//

/**
 * STDIO I/O context
 */
typedef struct {
    /**
     * IO source type;
     */
    uni_hal_io_context_t* io_context;

} uni_hal_stdio_context_t;



//
// Functions
//

/**
 * Initializes standard IO
 * @param io_ctx pointer to the I/O context
 * @return bool on success
 */
bool uni_hal_io_stdio_init(uni_hal_io_context_t *io_ctx);



#if defined(__cplusplus)
}
#endif
