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

uni_hal_pwr_context_t g_uni_hal_pwr_ctx = {
        .inited = false,
        .battery_charging = false,
};



//
// Functions
//

bool uni_hal_pwr_is_inited() {
    return g_uni_hal_pwr_ctx.inited;
}


bool uni_hal_pwr_is_battery_charging() {
    bool result = false;
    if (uni_hal_pwr_is_inited()) {
        result = g_uni_hal_pwr_ctx.battery_charging;
    }
    return result;
}
