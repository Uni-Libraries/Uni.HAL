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


bool uni_hal_pwr_set_battery_charging(bool value) {
    g_uni_hal_pwr_ctx.battery_charging = value;
    return true;
}
