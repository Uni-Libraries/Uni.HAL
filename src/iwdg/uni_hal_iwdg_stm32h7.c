//
// Includes
//

// stdlib
#include <stddef.h>

// STM
#include <stm32h7xx_ll_iwdg.h>

// Uni.Common
#include <uni_common.h>

// UNI_HAL
#include "iwdg/uni_hal_iwdg.h"
#include "systick/uni_hal_systick.h"


//
// Defines
//

/**
 * Longest wait for the prescaler and reload registers to take the new values
 */
#define UNI_HAL_IWDG_READY_TIMEOUT_MS (100U)



//
// Private
//

static IWDG_TypeDef *_uni_hal_iwdg_handle_get(uni_hal_core_periph_e instance) {
    IWDG_TypeDef *result;
    switch (instance) {
        case UNI_HAL_CORE_PERIPH_IWDG_1:
            result = IWDG1;
            break;
        default:
            result = NULL;
            break;
    }
    return result;
}



//
// Public Functions
//

bool uni_hal_iwdg_init(uni_hal_iwdg_context_t *ctx) {
    bool result = false;

    if (ctx != NULL) {
        IWDG_TypeDef *instance = _uni_hal_iwdg_handle_get(ctx->instance);
        if (instance != NULL) {

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
                    // unknown prescaler
                    break;
            }

            LL_IWDG_Enable(instance);
            LL_IWDG_EnableWriteAccess(instance);
            LL_IWDG_SetPrescaler(instance, prescaler);
            // the reload register is 12 bits wide
            LL_IWDG_SetReloadCounter(instance, uni_common_math_min(ctx->watchdog_counter, 0x0FFFU));
            // PVU/RVU clear a few LSI periods after the write; without LSI they never do
            uint32_t const start_ms = uni_hal_systick_get_ms();
            result = true;
            while (LL_IWDG_IsReady(instance) != 1) {
                if ((uni_hal_systick_get_ms() - start_ms) > UNI_HAL_IWDG_READY_TIMEOUT_MS) {
                    result = false;
                    break;
                }
            }

            // the watchdog is running in any case and has to be served
            ctx->inited = true;
            uni_hal_iwdg_reload(ctx);
        }
    }

    return result;
}


bool uni_hal_iwdg_deinit(uni_hal_iwdg_context_t *ctx) {
    (void) ctx;
    return false;
}


bool uni_hal_iwdg_reload(uni_hal_iwdg_context_t *ctx) {
    bool result = false;

    if (uni_hal_iwdg_is_inited(ctx)) {
        LL_IWDG_ReloadCounter(_uni_hal_iwdg_handle_get(ctx->instance));
        result = true;
    }

    return result;
}
