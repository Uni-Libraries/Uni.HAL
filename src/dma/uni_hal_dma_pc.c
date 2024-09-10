//
// Includes
//

#include "dma/uni_hal_dma.h"



//
// Functions
//

bool uni_hal_dma_init(uni_hal_dma_context_t *ctx){
    bool result = false;
    if(ctx!=nullptr){
        ctx->state.initialized = true;
        result = true;
    }
    return result;
}


bool uni_hal_dma_is_inited(const uni_hal_dma_context_t* ctx) {
    bool result = false;
    if (ctx != nullptr) {
        result = ctx->state.initialized;
    }
    return result;
}
