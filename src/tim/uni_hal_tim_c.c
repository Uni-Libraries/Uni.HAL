//
// Includes
//

// stdlib
#include <stddef.h>

// UNI_HAL
#include "tim/uni_hal_tim.h"


//
// Functions
//

bool uni_hal_tim_is_inited(const uni_hal_tim_context_t *ctx) {
    bool result = false;
    if(ctx != NULL){
        result = ctx->status.inited;
    }
    return result;
}
