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

bool uni_hal_iwdg_is_inited(const uni_hal_iwdg_context_t *ctx) {
    bool result = false;
    if(ctx!= NULL){
        result = ctx->inited;
    }
    return result;
}
