//
// Includes
//

// stdlib
#include <stddef.h>
#include <stdint.h>

// ST
#include <stm32h7xx_ll_tim.h>

// FreeRTOS
#include <FreeRTOS.h>

// uni_hal
#include "core/uni_hal_core.h"
#include "rcc/uni_hal_rcc.h"
#include "tim/uni_hal_tim.h"

//
// Defines
//

#define uni_hal_TIM_MAXTIMERS (17U)

//
// Globals
//

static uni_hal_tim_callback_fn _callback_fn[uni_hal_TIM_MAXTIMERS] = {0};
static uni_hal_tim_context_t *_callback_timctx[uni_hal_TIM_MAXTIMERS] = {NULL};
static void *_callback_userctx[uni_hal_TIM_MAXTIMERS] = {NULL};


//
// Private
//

static uint32_t _uni_hal_tim_get_number(TIM_TypeDef *instance) {
    uint32_t result;
    if (instance == TIM1) {
        result = 1U;
    } else if (instance == TIM2) {
        result = 2U;
    } else if (instance == TIM3) {
        result = 3U;
    } else if (instance == TIM4) {
        result = 4U;
    } else if (instance == TIM5) {
        result = 5U;
    } else if (instance == TIM6) {
        result = 6U;
    } else if (instance == TIM7) {
        result = 7U;
    } else if (instance == TIM8) {
        result = 8U;
    } else if (instance == TIM12) {
        result = 12U;
    } else if (instance == TIM13) {
        result = 13U;
    } else if (instance == TIM14) {
        result = 14U;
    } else if (instance == TIM15) {
        result = 15U;
    } else if (instance == TIM16) {
        result = 16U;
    } else if (instance == TIM17) {
        result = 17U;
    } else {
        result = 0U;
    }

    return result;
}


static TIM_TypeDef *_uni_hal_tim_get_handle(uni_hal_core_periph_e instance) {
    TIM_TypeDef *result;
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
        case UNI_HAL_CORE_PERIPH_TIM_12:
            result = TIM12;
            break;
        case UNI_HAL_CORE_PERIPH_TIM_13:
            result = TIM13;
            break;
        case UNI_HAL_CORE_PERIPH_TIM_14:
            result = TIM14;
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
            result = nullptr;
            break;
    }

    return result;
}

bool uni_hal_tim_period_elapsed(uni_hal_core_periph_e periph) {
    BaseType_t higher_task_woken = false;

    TIM_TypeDef *handle = _uni_hal_tim_get_handle(periph);
    if (handle != nullptr) {
        if (LL_TIM_IsActiveFlag_UPDATE(handle)) {
            LL_TIM_ClearFlag_UPDATE(handle);
            uint32_t id = _uni_hal_tim_get_number(handle);
            if (id > 0U) {
                uni_hal_tim_callback_fn fn = _callback_fn[id - 1];
                if (fn != nullptr) {
                    higher_task_woken = fn(_callback_timctx[id - 1], _callback_userctx[id - 1]);
                }
            }
        }
    }

    return higher_task_woken;
}



//
// Handlers
//



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
        uint32_t id = _uni_hal_tim_get_number(_uni_hal_tim_get_handle(ctx->config.instance));
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
            uni_hal_core_irq_enable(UNI_HAL_CORE_IRQ_TIM_15, 5, 0);
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
