//
// Includes
//

// stdlib
#include <stddef.h>
#include <stdint.h>

// ST
#include <stm32h7xx_ll_tim.h>

// uni_hal
#include "core/uni_hal_core.h"
#include "rcc/uni_hal_rcc.h"
#include "tim/uni_hal_tim.h"

// Each supported base timer has a driver-owned update vector in uni_hal_tim_i.c.
typedef struct {
    uni_hal_core_periph_e instance;
    TIM_TypeDef *handle;
    uni_hal_core_irq_e irq;
    uni_hal_tim_callback_fn callback;
    uni_hal_tim_context_t *timer_ctx;
    void *user_ctx;
} timer_driver_t;

static timer_driver_t timers[] = {
    {.instance = UNI_HAL_CORE_PERIPH_TIM_2,  .handle = TIM2,  .irq = UNI_HAL_CORE_IRQ_TIM_2},
    {.instance = UNI_HAL_CORE_PERIPH_TIM_15, .handle = TIM15, .irq = UNI_HAL_CORE_IRQ_TIM_15},
    {.instance = UNI_HAL_CORE_PERIPH_TIM_16, .handle = TIM16, .irq = UNI_HAL_CORE_IRQ_TIM_16},
    {.instance = UNI_HAL_CORE_PERIPH_TIM_17, .handle = TIM17, .irq = UNI_HAL_CORE_IRQ_TIM_17},
};

static timer_driver_t *find_timer(uni_hal_core_periph_e instance) {
    for (size_t i = 0; i < sizeof timers / sizeof timers[0]; ++i) {
        if (timers[i].instance == instance) {
            return &timers[i];
        }
    }
    return nullptr;
}

bool uni_hal_tim_period_elapsed(uni_hal_core_periph_e periph) {
    timer_driver_t *timer = find_timer(periph);
    if (timer == nullptr || !LL_TIM_IsEnabledIT_UPDATE(timer->handle) || !LL_TIM_IsActiveFlag_UPDATE(timer->handle)) {
        return false;
    }
    LL_TIM_ClearFlag_UPDATE(timer->handle);
    return timer->callback != nullptr && timer->callback(timer->timer_ctx, timer->user_ctx);
}

bool uni_hal_tim_init(uni_hal_tim_context_t *ctx) {
    if (ctx == nullptr) {
        return false;
    }
    ctx->status.inited = false;
    timer_driver_t *timer = find_timer(ctx->config.instance);
    if (timer == nullptr || ctx->config.channel_count != 0U || ctx->config.prescaler > UINT16_MAX ||
        (!IS_TIM_32B_COUNTER_INSTANCE(timer->handle) && ctx->config.reload_value > UINT16_MAX) ||
        !uni_hal_rcc_clk_set(ctx->config.instance, true)) {
        return false;
    }
    LL_TIM_InitTypeDef config = {
        .Prescaler = ctx->config.prescaler,
        .CounterMode = LL_TIM_COUNTERMODE_UP,
        .Autoreload = ctx->config.reload_value,
        .ClockDivision = LL_TIM_CLOCKDIVISION_DIV1,
        .RepetitionCounter = 0U,
    };
    ctx->status.inited = LL_TIM_Init(timer->handle, &config) == SUCCESS;
    return ctx->status.inited;
}

bool uni_hal_tim_register_callback(uni_hal_tim_context_t *ctx, uni_hal_tim_callback_fn callback, void *callback_ctx) {
    if (!uni_hal_tim_is_inited(ctx)) {
        return false;
    }
    timer_driver_t *timer = find_timer(ctx->config.instance);
    if (timer == nullptr) {
        return false;
    }
    timer->callback = callback;
    timer->timer_ctx = ctx;
    timer->user_ctx = callback_ctx;
    return true;
}

bool uni_hal_tim_start(uni_hal_tim_context_t *ctx) {
    if (!uni_hal_tim_is_inited(ctx)) {
        return false;
    }
    timer_driver_t *timer = find_timer(ctx->config.instance);
    if (timer == nullptr || !uni_hal_core_irq_enable(timer->irq, 5, 0)) {
        return false;
    }
    // LL_TIM_Init latches the prescaler with an update event, which leaves UIF set.
    LL_TIM_ClearFlag_UPDATE(timer->handle);
    LL_TIM_EnableIT_UPDATE(timer->handle);
    LL_TIM_EnableCounter(timer->handle);
    return true;
}

bool uni_hal_tim_stop(uni_hal_tim_context_t *ctx) {
    if (!uni_hal_tim_is_inited(ctx)) {
        return false;
    }
    timer_driver_t *timer = find_timer(ctx->config.instance);
    if (timer == nullptr) {
        return false;
    }
    LL_TIM_DisableCounter(timer->handle);
    LL_TIM_DisableIT_UPDATE(timer->handle);
    return true;
}
