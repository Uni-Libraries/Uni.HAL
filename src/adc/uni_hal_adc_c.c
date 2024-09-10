//
// Includes
//

#include "adc/uni_hal_adc.h"


//
// Functions
//

bool uni_hal_adc_is_inited(const uni_hal_adc_context_t *ctx) {
    return ctx != nullptr && ctx->state.initialized != false;
}
