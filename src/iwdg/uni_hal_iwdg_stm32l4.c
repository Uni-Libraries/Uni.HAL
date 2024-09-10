//
// Includes
//

// stdlib
#include <stddef.h>

// STM
#include <stm32l4xx_ll_iwdg.h>

// Uni.Common
#include <uni_common.h>

// Uni.HAL
#include "iwdg/uni_hal_iwdg.h"
#include "gpio/uni_hal_gpio.h"



//
// Public Functions
//

bool uni_hal_iwdg_init(uni_hal_iwdg_context_t *ctx) {
    bool result = false;

    if (ctx != NULL) {

        uint32_t prescaler = LL_IWDG_PRESCALER_4;
        switch (ctx->watchdog_prescaler) {
        case 4:
            prescaler = LL_IWDG_PRESCALER_4;
            break;
        case 8:
            prescaler = LL_IWDG_PRESCALER_8;
            break;
        case 16:
            prescaler = LL_IWDG_PRESCALER_16;
            break;
        case 32:
            prescaler = LL_IWDG_PRESCALER_32;
            break;
        case 64:
            prescaler = LL_IWDG_PRESCALER_64;
            break;
        case 128:
            prescaler = LL_IWDG_PRESCALER_128;
            break;
        case 256:
            prescaler = LL_IWDG_PRESCALER_256;
            break;
        default:
            break;
        }

        uint32_t counter = uni_common_math_min(ctx->watchdog_counter, 0x0FFF);

        LL_IWDG_Enable(IWDG);
        LL_IWDG_EnableWriteAccess(IWDG);
        LL_IWDG_SetPrescaler(IWDG, prescaler);
        LL_IWDG_SetReloadCounter(IWDG, counter);
        while (LL_IWDG_IsReady(IWDG) != 1) {
        }

        result = true;
        ctx->inited = true;
        uni_hal_iwdg_reload(ctx);
    }

    return result;
}


bool uni_hal_iwdg_deinit(uni_hal_iwdg_context_t *ctx) {
    bool result = false;
    if(ctx!=NULL){
        ctx->inited = false;
        result = true;
    }
    return result;
}


bool uni_hal_iwdg_reload(uni_hal_iwdg_context_t *ctx) {
    bool result = false;

    if (uni_hal_iwdg_is_inited(ctx)) {
        LL_IWDG_ReloadCounter(IWDG);
        result = true;
    }

    return result;
}
