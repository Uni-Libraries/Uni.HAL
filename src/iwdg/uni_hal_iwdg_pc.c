//
// Includes
//

// stdlib
#include <stddef.h>

// uni_hal
#include "iwdg/uni_hal_iwdg.h"


//
// Functions
//

bool uni_hal_iwdg_init(uni_hal_iwdg_context_t *ctx) {
    bool result = false;
    if (ctx != NULL) {
        ctx->inited = true;
        result = true;
    }
    return result;
}


bool uni_hal_iwdg_deinit(uni_hal_iwdg_context_t *ctx) {
    bool result = false;
    if (ctx != NULL) {
        ctx->inited = false;
        result = true;
    }
    return result;
}

bool uni_hal_iwdg_reload(uni_hal_iwdg_context_t *ctx) {
    bool result = false;
    if (uni_hal_iwdg_is_inited(ctx)) {
        result = true;
    }
    return result;
}
