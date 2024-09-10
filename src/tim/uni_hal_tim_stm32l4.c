//
// Includes
//

// stdlib
#include <stddef.h>
#include <stdint.h>

// ST
#include <stm32l4xx_ll_tim.h>

// Uni.Common
#include <uni_common.h>

// Uni.HAL
#include "core/uni_hal_core.h"
#include "rcc/uni_hal_rcc.h"
#include "tim/uni_hal_tim.h"

//
// Defines
//

#define UNI_HAL_TIM_MAXTIMERS (17U)

//
// Globals
//

static uni_hal_tim_callback_fn _callback_fn[UNI_HAL_TIM_MAXTIMERS] = {0};
static uni_hal_tim_context_t *_callback_timctx[UNI_HAL_TIM_MAXTIMERS] = {NULL};
static void *_callback_userctx[UNI_HAL_TIM_MAXTIMERS] = {NULL};


//
// Private
//

static uint32_t _uni_hal_tim_get_number(uni_hal_core_periph_e instance) {
    uint32_t result = 0U;
    switch (instance) {
    case UNI_HAL_CORE_PERIPH_TIM_1:
        result = 1U;
        break;
    case UNI_HAL_CORE_PERIPH_TIM_2:
        result = 2U;
        break;
    case UNI_HAL_CORE_PERIPH_TIM_3:
        result = 3U;
        break;
    case UNI_HAL_CORE_PERIPH_TIM_4:
        result = 4U;
        break;
    case UNI_HAL_CORE_PERIPH_TIM_5:
        result = 5U;
        break;
    case UNI_HAL_CORE_PERIPH_TIM_6:
        result = 6U;
        break;
    case UNI_HAL_CORE_PERIPH_TIM_7:
        result = 7U;
        break;
    case UNI_HAL_CORE_PERIPH_TIM_8:
        result = 8U;
        break;
    case UNI_HAL_CORE_PERIPH_TIM_15:
        result = 15U;
        break;
    case UNI_HAL_CORE_PERIPH_TIM_16:
        result = 16U;
        break;
    case UNI_HAL_CORE_PERIPH_TIM_17:
        result = 17U;
        break;
    default:
        break;
    }

    return result;
}

static uint32_t _uni_hal_tim_get_number_by_handle(TIM_TypeDef *instance) {
    uint32_t result = 0U;
    if (instance == TIM1) {
        result = 1U;
    }
    else if (instance == TIM2) {
        result = 2U;
    }
    else if (instance == TIM3) {
        result = 3U;
    }
    else if (instance == TIM4) {
        result = 4U;
    }
    else if (instance == TIM5) {
        result = 5U;
    }
    else if (instance == TIM6) {
        result = 6U;
    }
    else if (instance == TIM7) {
        result = 7U;
    }
    else if (instance == TIM8) {
        result = 8U;
    }
    else if (instance == TIM15) {
        result = 15U;
    }
    else if (instance == TIM16) {
        result = 16U;
    }
    else if (instance == TIM17) {
        result = 17U;
    }
    else {
        result = 0U;
    }

    return result;
}


static TIM_TypeDef *_uni_hal_tim_get_handle(uni_hal_core_periph_e instance) {
    TIM_TypeDef *result = NULL;
    switch (instance) {
    case UNI_HAL_CORE_PERIPH_TIM_1:
        result = TIM1;
        break;
    case UNI_HAL_CORE_PERIPH_TIM_2:
        result = TIM2;
        break;
    case UNI_HAL_CORE_PERIPH_TIM_3:
        result = TIM3;
        break;
    case UNI_HAL_CORE_PERIPH_TIM_4:
        result = TIM4;
        break;
    case UNI_HAL_CORE_PERIPH_TIM_5:
        result = TIM5;
        break;
    case UNI_HAL_CORE_PERIPH_TIM_6:
        result = TIM6;
        break;
    case UNI_HAL_CORE_PERIPH_TIM_7:
        result = TIM7;
        break;
    case UNI_HAL_CORE_PERIPH_TIM_8:
        result = TIM8;
        break;
    case UNI_HAL_CORE_PERIPH_TIM_15:
        result = TIM15;
        break;
    case UNI_HAL_CORE_PERIPH_TIM_16:
        result = TIM16;
        break;
    case UNI_HAL_CORE_PERIPH_TIM_17:
        result = TIM17;
        break;
    default:
        break;
    }

    return result;
}

static void _uni_hal_tim_period_elapsed(TIM_TypeDef *handle) {
    if (handle != NULL) {
        if (LL_TIM_IsActiveFlag_UPDATE(handle)) {
            LL_TIM_ClearFlag_UPDATE(handle);
            uint32_t id = _uni_hal_tim_get_number_by_handle(handle);
            if (id > 0U) {
                uni_hal_tim_callback_fn fn = _callback_fn[id - 1];
                if (fn != NULL) {
                    fn(_callback_timctx[id - 1], _callback_userctx[id - 1]);
                }
            }
        }
    }
}


//
// Handlers
//

UNI_COMMON_COMPILER_WEAK void TIM7_IRQHandler(void) {
    _uni_hal_tim_period_elapsed(TIM7);
}



//
// Functions
//

bool uni_hal_tim_init(uni_hal_tim_context_t *ctx) {
    bool result = false;
    if (ctx != NULL) {
        result = uni_hal_rcc_clk_set(ctx->config.instance, true);

        TIM_TypeDef *handle = _uni_hal_tim_get_handle(ctx->config.instance);
        if (handle != NULL) {
            LL_TIM_InitTypeDef tim_init;
            tim_init.Prescaler = ctx->config.prescaler;
            tim_init.CounterMode = LL_TIM_COUNTERMODE_UP;
            tim_init.Autoreload = ctx->config.period;
            tim_init.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
            tim_init.RepetitionCounter = 0x00;
            result = LL_TIM_Init(handle, &tim_init) == SUCCESS;
        }

        ctx->status.inited = result;
    }

    return result;
}

bool uni_hal_tim_register_callback(uni_hal_tim_context_t *ctx, uni_hal_tim_callback_fn callback, void *callback_ctx) {
    bool result = false;

    if (uni_hal_tim_is_inited(ctx)) {
        uint32_t id = _uni_hal_tim_get_number(ctx->config.instance);
        if (id > 0U) {
            _callback_fn[id - 1] = callback;
            _callback_timctx[id - 1] = ctx;
            _callback_userctx[id - 1] = callback_ctx;

            result = true;
        }
    }

    return result;
}


bool uni_hal_tim_start(uni_hal_tim_context_t *ctx) {
    bool result = false;

    if (uni_hal_tim_is_inited(ctx)) {
        TIM_TypeDef *handle = _uni_hal_tim_get_handle(ctx->config.instance);
        if (handle != NULL) {
            LL_TIM_EnableIT_UPDATE(handle);
            LL_TIM_EnableCounter(handle);
            result = true;
        }
    }

    return result;
}


bool uni_hal_tim_stop(uni_hal_tim_context_t *ctx) {
    bool result = false;

    if (uni_hal_tim_is_inited(ctx)) {
        TIM_TypeDef *handle = _uni_hal_tim_get_handle(ctx->config.instance);
        if (handle != NULL) {
            LL_TIM_DisableCounter(handle);
            LL_TIM_DisableIT_UPDATE(handle);
            result = true;
        }
    }

    return result;
}
