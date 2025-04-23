//
// Includes
//

// stdlib
#include <stddef.h>
#include <stdint.h>
#include <math.h>

// ST
#include <stm32l4xx_ll_tim.h>

// Uni.Common
#include <uni_common.h>

// Uni.HAL
#include "core/uni_hal_core.h"
#include "rcc/uni_hal_rcc.h"
#include "systick/uni_hal_systick.h"
#include "tim/uni_hal_tim.h"

//
// Defines
//

#define UNI_HAL_TIM_MAXTIMERS (17U)

//
// Globals
//

static uni_hal_tim_context_t *g_uni_hal_tim_ctx[UNI_HAL_TIM_MAXTIMERS] = {nullptr};

static uni_hal_tim_callback_fn _callback_fn[UNI_HAL_TIM_MAXTIMERS] = {0};

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

static uint32_t _uni_hal_tim_get_channel(uni_hal_tim_channel_num_e channel)
{
    uint32_t result = 0U;
    switch (channel)
    {
    case UNI_HAL_TIM_CHANNEL_1:
        result = LL_TIM_CHANNEL_CH1;
        break;
    case UNI_HAL_TIM_CHANNEL_2:
        result = LL_TIM_CHANNEL_CH2;
        break;
    case UNI_HAL_TIM_CHANNEL_3:
        result = LL_TIM_CHANNEL_CH3;
        break;
    case UNI_HAL_TIM_CHANNEL_4:
        result = LL_TIM_CHANNEL_CH4;
        break;
    case UNI_HAL_TIM_CHANNEL_5:
        result = LL_TIM_CHANNEL_CH5;
        break;
    case UNI_HAL_TIM_CHANNEL_6:
        result = LL_TIM_CHANNEL_CH6;
        break;
    default:
        break;
    }
    return result;
}

static uint32_t _uni_hal_tim_get_polarity(uni_hal_tim_polarity_e channel)
{
    uint32_t result = 0U;
    switch (channel)
    {
    case UNI_HAL_TIM_POLARITY_RISING:
        result = LL_TIM_IC_POLARITY_RISING;
        break;
    case UNI_HAL_TIM_POLARITY_FALLING:
        result = LL_TIM_IC_POLARITY_FALLING;
        break;
    case UNI_HAL_TIM_POLARITY_BOTH:
        result = LL_TIM_IC_POLARITY_BOTHEDGE;
        break;
    default:
        break;
    }
    return result;
}

static void _uni_hal_tim_irq_cc(TIM_TypeDef* handle)
{
    uint32_t tim_cnt;
    uint32_t tim_ms = uni_hal_systick_get_ms();
    uint32_t tim_id = _uni_hal_tim_get_number_by_handle(handle) - 1;
    uni_hal_tim_context_t* ctx = g_uni_hal_tim_ctx[tim_id];

    if (LL_TIM_IsActiveFlag_CC1(handle)) {
        tim_cnt = LL_TIM_IC_GetCaptureCH1(handle);
        ctx->status.chan_val[UNI_HAL_TIM_CHANNEL_1] = tim_cnt - ctx->status.chan_cnt[UNI_HAL_TIM_CHANNEL_1];
        ctx->status.chan_cnt[UNI_HAL_TIM_CHANNEL_1] = tim_cnt;
        ctx->status.chan_ms[UNI_HAL_TIM_CHANNEL_1]  = tim_ms;
        LL_TIM_ClearFlag_CC1(handle);
    }

    if (LL_TIM_IsActiveFlag_CC2(handle)) {
        tim_cnt = LL_TIM_IC_GetCaptureCH2(handle);
        ctx->status.chan_val[UNI_HAL_TIM_CHANNEL_2] = tim_cnt - ctx->status.chan_cnt[UNI_HAL_TIM_CHANNEL_2];
        ctx->status.chan_cnt[UNI_HAL_TIM_CHANNEL_2] = tim_cnt;
        ctx->status.chan_ms[UNI_HAL_TIM_CHANNEL_2]  = tim_ms;
        LL_TIM_ClearFlag_CC2(handle);
    }

    if (LL_TIM_IsActiveFlag_CC3(handle)) {
        tim_cnt = LL_TIM_IC_GetCaptureCH3(handle);
        ctx->status.chan_val[UNI_HAL_TIM_CHANNEL_3] = tim_cnt - ctx->status.chan_cnt[UNI_HAL_TIM_CHANNEL_3];
        ctx->status.chan_cnt[UNI_HAL_TIM_CHANNEL_3] = tim_cnt;
        ctx->status.chan_ms[UNI_HAL_TIM_CHANNEL_3]  = tim_ms;
        LL_TIM_ClearFlag_CC3(handle);
    }

    if (LL_TIM_IsActiveFlag_CC4(handle)) {
        tim_cnt = LL_TIM_IC_GetCaptureCH4(handle);
        ctx->status.chan_val[UNI_HAL_TIM_CHANNEL_4] = tim_cnt - ctx->status.chan_cnt[UNI_HAL_TIM_CHANNEL_4];
        ctx->status.chan_cnt[UNI_HAL_TIM_CHANNEL_4] = tim_cnt;
        ctx->status.chan_ms[UNI_HAL_TIM_CHANNEL_4]  = tim_ms;
        LL_TIM_ClearFlag_CC4(handle);
    }
}


static void _uni_hal_tim_period_elapsed(TIM_TypeDef *handle) {
    if (handle != NULL) {
        if (LL_TIM_IsActiveFlag_UPDATE(handle)) {
            LL_TIM_ClearFlag_UPDATE(handle);
            uint32_t id = _uni_hal_tim_get_number_by_handle(handle);
            if (id > 0U) {
                uni_hal_tim_callback_fn fn = _callback_fn[id - 1];
                if (fn != NULL) {
                    fn(g_uni_hal_tim_ctx[id - 1], _callback_userctx[id - 1]);
                }
            }
        }
    }
}

static bool _uni_hal_tim_init_channel(uni_hal_tim_context_t* ctx, uni_hal_tim_channel_t* channel)
{
    bool result = false;
    if (ctx != nullptr && channel != nullptr)
    {
        result = true;
        if (channel->gpio)
        {
            result = uni_hal_gpio_pin_init(channel->gpio);
        }

        if (channel->type == UNI_HAL_TIM_TYPE_INPUTCAPTURE)
        {
            LL_TIM_IC_InitTypeDef ic_config = {
                .ICPolarity = _uni_hal_tim_get_polarity(channel->polarity),
                .ICActiveInput = LL_TIM_ACTIVEINPUT_DIRECTTI,
                .ICPrescaler = LL_TIM_ICPSC_DIV1,
                .ICFilter = LL_TIM_IC_FILTER_FDIV1,
            };
            result = result && (LL_TIM_IC_Init(_uni_hal_tim_get_handle(ctx->config.instance), _uni_hal_tim_get_channel(channel->channel_number), &ic_config) == SUCCESS);
        }
        else
        {
            result = false;
        }
    }

    return result;
}


//
// Handlers
//

void TIM1_CC_IRQHandler(void) {
    _uni_hal_tim_irq_cc(TIM1);
}

UNI_COMMON_COMPILER_WEAK void TIM7_IRQHandler(void) {
    _uni_hal_tim_period_elapsed(TIM7);
}



//
// Functions
//

bool uni_hal_tim_init(uni_hal_tim_context_t *ctx) {
    bool result = false;
    if (ctx != NULL)
    {
        result = uni_hal_rcc_clk_set(ctx->config.instance, true);

        TIM_TypeDef *handle = _uni_hal_tim_get_handle(ctx->config.instance);
        if (handle != NULL) {
            LL_TIM_InitTypeDef tim_init;
            tim_init.Prescaler = ctx->config.prescaler;
            tim_init.CounterMode = LL_TIM_COUNTERMODE_UP;
            tim_init.Autoreload = ctx->config.reload_value;
            tim_init.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
            tim_init.RepetitionCounter = 0x00;
            result = LL_TIM_Init(handle, &tim_init) == SUCCESS;
        }

        if (ctx->config.channel_count > 0 && ctx->config.channel != nullptr)
        {
            for (size_t i = 0; i < ctx->config.channel_count; i++)
            {
                result = result && _uni_hal_tim_init_channel(ctx, ctx->config.channel[i]);
            }
        }

        g_uni_hal_tim_ctx[_uni_hal_tim_get_number(ctx->config.instance) - 1] = ctx;
        ctx->status.inited = result;
    }

    return result;
}

bool uni_hal_tim_register_callback(uni_hal_tim_context_t *ctx, uni_hal_tim_callback_fn callback, void *callback_ctx) {
    bool result = false;

    if (uni_hal_tim_is_inited(ctx)) {
        uint32_t id = _uni_hal_tim_get_number(ctx->config.instance);
        if (id > 0U) {
            if (g_uni_hal_tim_ctx[id-1] == ctx)
            {
                _callback_fn[id - 1] = callback;
                _callback_userctx[id - 1] = callback_ctx;
                result = true;
            }
        }
    }

    return result;
}


bool uni_hal_tim_start(uni_hal_tim_context_t *ctx) {
    bool result = false;

    if (uni_hal_tim_is_inited(ctx)) {
        TIM_TypeDef *handle = _uni_hal_tim_get_handle(ctx->config.instance);
        if (handle != nullptr) {
            for (size_t i = 0; i < ctx->config.channel_count; i++)
            {
                if (ctx->config.channel[i]->type == UNI_HAL_TIM_TYPE_INPUTCAPTURE)
                {
                    switch (ctx->config.channel[i]->channel_number)
                    {
                    case UNI_HAL_TIM_CHANNEL_1:
                        LL_TIM_EnableIT_CC1(handle);
                        break;
                    case UNI_HAL_TIM_CHANNEL_2:
                        LL_TIM_EnableIT_CC2(handle);
                        break;
                    case UNI_HAL_TIM_CHANNEL_3:
                        LL_TIM_EnableIT_CC3(handle);
                        break;
                    case UNI_HAL_TIM_CHANNEL_4:
                        LL_TIM_EnableIT_CC4(handle);
                        break;
                    default:
                        break;
                    }
                    LL_TIM_CC_EnableChannel(handle,  _uni_hal_tim_get_channel(ctx->config.channel[i]->channel_number));
                }
            }

            uni_hal_core_irq_enable(UNI_HAL_CORE_IRQ_TIM_1, 5, 0);
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

uint32_t uni_hal_tim_get_period_us(uni_hal_tim_context_t *ctx)
{
    uint32_t result = 0U;

    if (uni_hal_tim_is_inited(ctx))
    {
        result = uni_hal_rcc_clk_get_freq(ctx->config.instance);
        result = result / (ctx->config.prescaler + 1);
        if (result)
        {
            result = 1'000'000 / result;
        }
        else
        {
            result = 0;
        }
    }

    return result;
}

uint32_t uni_hal_tim_get_chan_freq(uni_hal_tim_context_t* ctx, uni_hal_tim_channel_num_e chan)
{
    uint32_t result = 0U;
    if (uni_hal_tim_is_inited(ctx))
    {
        result = ctx->status.chan_val[chan] * uni_hal_tim_get_period_us(ctx);
        if (result)
        {
            result = (uint32_t)round(1'000'000.f / result);
        }
    }

    return result;
}

uint32_t uni_hal_tim_get_chan_age(uni_hal_tim_context_t* ctx, uni_hal_tim_channel_num_e chan)
{
    uint32_t result = UINT32_MAX;
    if (uni_hal_tim_is_inited(ctx))
    {
        result = uni_hal_systick_get_ms() - ctx->status.chan_ms[chan];
    }
    return result;
}
