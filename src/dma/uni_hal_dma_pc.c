//
// Includes
//

#include "dma/uni_hal_dma.h"



//
// Functions
//

bool uni_hal_dma_init(uni_hal_dma_context_t *ctx){
    bool result = false;
    if(ctx!=NULL){
        ctx->state.initialized = true;
        result = true;
    }
    return result;
}
