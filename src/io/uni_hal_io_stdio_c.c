//
// Includes
//

// uni_hal
#include "uni_common.h"
#include "uni_hal_io_stdio.h"



//
// Globals
//

uni_hal_stdio_context_t g_uni_hal_io_stdio_ctx = {.io_context = NULL};



//
// Public functions
//

bool uni_hal_io_stdio_init(uni_hal_io_context_t *io_ctx) {
    g_uni_hal_io_stdio_ctx.io_context = io_ctx;
    return true;
}
