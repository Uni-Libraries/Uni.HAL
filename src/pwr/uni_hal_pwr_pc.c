//
// Includes
//

// stdlib
#include <stddef.h>

// uni_hal
#include "pwr/uni_hal_pwr.h"


//
// Globals
//

extern uni_hal_pwr_context_t g_uni_hal_pwr_ctx;



//
// Functions
//

void uni_hal_pwr_reset(void){

}


bool uni_hal_pwr_init() {
    g_uni_hal_pwr_ctx.inited = true;
    return g_uni_hal_pwr_ctx.inited;
}

