//
// Includes
//

// stdlib
#include <stddef.h>

// uni_hal
#include "crc/uni_hal_hal_crc.h"



//
// Functions
//

bool uni_hal_crc_is_inited(const uni_hal_crc_context_t *ctx) {
    bool result = false;
    if (ctx != NULL) {
        result = ctx->initialized;
    }
    return result;
}
