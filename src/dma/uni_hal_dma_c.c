//
// Includes
//

#include "dma/uni_hal_dma.h"



//
// Functions
//

bool uni_hal_dma_is_inited(const uni_hal_dma_context_t *ctx) {
    return ctx != nullptr && ctx->state.initialized != false;
}
