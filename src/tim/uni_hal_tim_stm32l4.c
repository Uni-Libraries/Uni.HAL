//
// Includes
//

// stdlib
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// ST
#include <stm32l4xx_ll_tim.h>

// Uni.Common
#include <uni_common.h>

// Uni.HAL
#include "core/uni_hal_core.h"
#include "core/uni_hal_core_enum.h"
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

static uni_hal_tim_context_t *g_uni_hal_tim_ctx[UNI_HAL_TIM_MAXTIMERS] = {NULL};

static uni_hal_tim_callback_fn _callback_fn[UNI_HAL_TIM_MAXTIMERS] = {0};

static void *_callback_userctx[UNI_HAL_TIM_MAXTIMERS] = {NULL};


//
// Private/LL
//

static uint32_t _uni_hal_tim_ll_capture_get(TIM_TypeDef *handle, uni_hal_tim_channel_num_e chan){
    uint32_t result = UINT32_MAX;

    switch(chan){
        case UNI_HAL_TIM_CHANNEL_1:
            result = LL_TIM_IC_GetCaptureCH1(handle);
            break;
        case UNI_HAL_TIM_CHANNEL_2:
            result = LL_TIM_IC_GetCaptureCH2(handle);
            break;
        case UNI_HAL_TIM_CHANNEL_3:
            result = LL_TIM_IC_GetCaptureCH3(handle);
            break;
        case UNI_HAL_TIM_CHANNEL_4:
            result = LL_TIM_IC_GetCaptureCH4(handle);
            break;
        default:
            break;
    }

    return result;
}


static void _uni_hal_tim_ll_overcapture_clear(TIM_TypeDef *handle, uni_hal_tim_channel_num_e chan){
    switch(chan){
        case UNI_HAL_TIM_CHANNEL_1:
            LL_TIM_ClearFlag_CC1OVR(handle);
            break;
        case UNI_HAL_TIM_CHANNEL_2:
            LL_TIM_ClearFlag_CC2OVR(handle);
            break;
        case UNI_HAL_TIM_CHANNEL_3:
            LL_TIM_ClearFlag_CC3OVR(handle);
            break;
        case UNI_HAL_TIM_CHANNEL_4:
            LL_TIM_ClearFlag_CC4OVR(handle);
            break;
        default:
            break;
    }
}


static void _uni_hal_tim_ll_capture_clear(TIM_TypeDef *handle, uni_hal_tim_channel_num_e chan){
    switch(chan){
        case UNI_HAL_TIM_CHANNEL_1:
            LL_TIM_ClearFlag_CC1(handle);
            break;
        case UNI_HAL_TIM_CHANNEL_2:
            LL_TIM_ClearFlag_CC2(handle);
            break;
        case UNI_HAL_TIM_CHANNEL_3:
            LL_TIM_ClearFlag_CC3(handle);
            break;
        case UNI_HAL_TIM_CHANNEL_4:
            LL_TIM_ClearFlag_CC4(handle);
            break;
        default:
            break;
    }
}

static void _uni_hal_tim_ll_capture_it_set(TIM_TypeDef *handle, uni_hal_tim_channel_num_e chan, bool val){
    switch(chan){
        case UNI_HAL_TIM_CHANNEL_1:
            val ? LL_TIM_EnableIT_CC1(handle) : LL_TIM_DisableIT_CC1(handle);
            break;
        case UNI_HAL_TIM_CHANNEL_2:
            val ? LL_TIM_EnableIT_CC2(handle) : LL_TIM_DisableIT_CC2(handle);
            break;
        case UNI_HAL_TIM_CHANNEL_3:
            val ? LL_TIM_EnableIT_CC3(handle) : LL_TIM_DisableIT_CC3(handle);
            break;
        case UNI_HAL_TIM_CHANNEL_4:
            val ? LL_TIM_EnableIT_CC4(handle) : LL_TIM_DisableIT_CC4(handle);
            break;
        default:
            break;
    }
}


static bool _uni_hal_tim_ll_overcapture_get(TIM_TypeDef *handle, uni_hal_tim_channel_num_e chan){
    bool result = false;

    switch(chan){
        case UNI_HAL_TIM_CHANNEL_1:
            result = LL_TIM_IsActiveFlag_CC1OVR(handle);
            break;
        case UNI_HAL_TIM_CHANNEL_2:
            result = LL_TIM_IsActiveFlag_CC2OVR(handle);
            break;
        case UNI_HAL_TIM_CHANNEL_3:
            result = LL_TIM_IsActiveFlag_CC3OVR(handle);
            break;
        case UNI_HAL_TIM_CHANNEL_4:
            result = LL_TIM_IsActiveFlag_CC4OVR(handle);
            break;
        default:
            break;
    }

    return result;
}



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




typedef struct
{
    uint32_t counter;
    uint32_t overflow_count;
} _uni_hal_tim_capture_t;


static uint32_t _uni_hal_tim_get_counter_period(TIM_TypeDef *handle)
{
    uint32_t result = 0U;
    if (handle != NULL)
    {
        uint32_t reload = LL_TIM_GetAutoReload(handle);
        if (reload < UINT32_MAX)
        {
            result = reload + 1U;
        }
    }

    return result;
}


static _uni_hal_tim_capture_t _uni_hal_tim_get_capture(TIM_TypeDef *handle, uni_hal_tim_context_t* ctx, uint32_t counter)
{
    _uni_hal_tim_capture_t result = {
        .counter = counter,
        .overflow_count = ctx->status.overflow_count,
    };

    uint32_t period = _uni_hal_tim_get_counter_period(handle);

    if (period > 0U)
    {
        uint32_t half_period = period / 2U;
        uint32_t current_counter = LL_TIM_GetCounter(handle);

        if (LL_TIM_IsActiveFlag_UPDATE(handle))
        {
            if (counter < half_period)
            {
                result.overflow_count++;
            }
        }
        else if (result.overflow_count > 0U && current_counter < half_period && counter > half_period)
        {
            result.overflow_count--;
        }
    }

    return result;
}


static bool _uni_hal_tim_get_capture_delta(TIM_TypeDef *handle, const _uni_hal_tim_capture_t* current, const uni_hal_tim_channel_state_t* previous, uint32_t* delta)
{
    bool result = false;

    if (handle != NULL && current != NULL && previous != NULL && delta != NULL)
    {
        uint64_t delta64 = 0U;
        uint32_t period = _uni_hal_tim_get_counter_period(handle);
        uint32_t overflow_delta = current->overflow_count - previous->overflow_count;

        if (period == 0U)
        {
            delta64 = (uint32_t)(current->counter - previous->counter);
        }
        else if (current->counter >= previous->counter)
        {
            delta64 = ((uint64_t)overflow_delta * period) + (current->counter - previous->counter);
        }
        else if (overflow_delta > 0U)
        {
            delta64 = ((uint64_t)(overflow_delta - 1U) * period) + (period - previous->counter) + current->counter;
        }

        if (delta64 > 0U && delta64 <= UINT32_MAX)
        {
            *delta = (uint32_t)delta64;
            result = true;
        }
    }

    return result;
}


static bool _uni_hal_tim_init_channel(uni_hal_tim_context_t* ctx, uni_hal_tim_channel_t* channel)
{
    bool result = false;
    if (ctx != NULL && channel != NULL)
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
                .ICFilter = (channel->filter & 0b1111) << 16U,
            };
            result = result && (LL_TIM_IC_Init(_uni_hal_tim_get_handle(ctx->config.instance), _uni_hal_tim_get_channel(channel->channel_number), &ic_config) == SUCCESS);
        }
        else if (channel->type == UNI_HAL_TIM_TYPE_OUTPUTCOMPARE)
        {
            LL_TIM_OC_InitTypeDef oc_config = {
                .OCMode = LL_TIM_OCMODE_TOGGLE,
                .OCState = LL_TIM_OCSTATE_DISABLE,
                .OCNState = LL_TIM_OCSTATE_DISABLE,
                .CompareValue = 0,
                .OCPolarity = LL_TIM_OCPOLARITY_HIGH,
                .OCNPolarity = LL_TIM_OCPOLARITY_HIGH,
                .OCIdleState = LL_TIM_OCIDLESTATE_LOW,
                .OCNIdleState = LL_TIM_OCIDLESTATE_LOW
            };
            result = result && (LL_TIM_OC_Init(_uni_hal_tim_get_handle(ctx->config.instance), _uni_hal_tim_get_channel(channel->channel_number), &oc_config) == SUCCESS);
        }
        else
        {
            result = false;
        }
    }

    return result;
}

static bool _uni_hal_tim_prescaler_valid(uint32_t value){
    return value <= UINT16_MAX;
}


static bool _uni_hal_tim_reloadvalue_valid(uni_hal_core_periph_e periph, uint32_t value){
    bool result = false;

    switch(periph){
        case UNI_HAL_CORE_PERIPH_TIM_1:
        case UNI_HAL_CORE_PERIPH_TIM_3:
        case UNI_HAL_CORE_PERIPH_TIM_4:
        case UNI_HAL_CORE_PERIPH_TIM_6:
        case UNI_HAL_CORE_PERIPH_TIM_7:
        case UNI_HAL_CORE_PERIPH_TIM_8:
        case UNI_HAL_CORE_PERIPH_TIM_15:
        case UNI_HAL_CORE_PERIPH_TIM_16:
        case UNI_HAL_CORE_PERIPH_TIM_17:
            result = value <= UINT16_MAX;
            break;
        case UNI_HAL_CORE_PERIPH_TIM_2:
        case UNI_HAL_CORE_PERIPH_TIM_5:
            result = true;
            break;
        default:
            break;
    }

    return result;
}



//
// Handlers/CC
//

static void _uni_hal_tim_irq_cc_chan(uni_hal_tim_context_t* ctx, TIM_TypeDef* handle, uni_hal_tim_channel_num_e chan)
{
    uni_hal_tim_channel_state_t* chan_st = &ctx->status.chan[chan];

    if(_uni_hal_tim_ll_overcapture_get(handle, chan)){
        chan_st->seen = false;
        chan_st->valid = false;
        _uni_hal_tim_ll_overcapture_clear(handle, chan);
    }
    else{
        uint32_t value = _uni_hal_tim_ll_capture_get(handle, chan);
        _uni_hal_tim_capture_t capture = _uni_hal_tim_get_capture(handle, ctx, value);

        if(chan_st->seen){
            uint32_t delta = 0U;
            bool delta_valid = _uni_hal_tim_get_capture_delta(handle, &capture, chan_st, &delta);
            chan_st->counter = capture.counter;
            chan_st->overflow_count = capture.overflow_count;

            if (delta_valid) {
                chan_st->period = delta;
                chan_st->timestamp = uni_hal_systick_get_ms();
                chan_st->valid = true;
            }
        }
        else{
            chan_st->counter = capture.counter;
            chan_st->overflow_count = capture.overflow_count;
            chan_st->seen = true;
        }
    }
}

static void _uni_hal_tim_irq_cc(TIM_TypeDef* handle)
{
    uint32_t tim_number = _uni_hal_tim_get_number_by_handle(handle);
    if (tim_number > 0U && tim_number <= UNI_HAL_TIM_MAXTIMERS) {
        uni_hal_tim_context_t* ctx = g_uni_hal_tim_ctx[tim_number - 1U];
        if (ctx != NULL) {
            if (LL_TIM_IsActiveFlag_CC1(handle)) {
                _uni_hal_tim_irq_cc_chan(ctx, handle, UNI_HAL_TIM_CHANNEL_1);
                LL_TIM_ClearFlag_CC1(handle);
            }

            if (LL_TIM_IsActiveFlag_CC2(handle)) {
                _uni_hal_tim_irq_cc_chan(ctx, handle, UNI_HAL_TIM_CHANNEL_2);
                LL_TIM_ClearFlag_CC2(handle);
            }

            if (LL_TIM_IsActiveFlag_CC3(handle)) {
                _uni_hal_tim_irq_cc_chan(ctx, handle, UNI_HAL_TIM_CHANNEL_3);
                LL_TIM_ClearFlag_CC3(handle);
            }

            if (LL_TIM_IsActiveFlag_CC4(handle)) {
                _uni_hal_tim_irq_cc_chan(ctx, handle, UNI_HAL_TIM_CHANNEL_4);
                LL_TIM_ClearFlag_CC4(handle);
            }

            // CC5 / CC6 cannot be used here
        }
    }
}



//
// Handlers/Update
//

static void _uni_hal_tim_irq_update(TIM_TypeDef* handle){
    uint32_t tim_number = _uni_hal_tim_get_number_by_handle(handle);
    if (tim_number > 0U && tim_number <= UNI_HAL_TIM_MAXTIMERS) {
        uni_hal_tim_context_t* ctx = g_uni_hal_tim_ctx[tim_number - 1U];
        if (ctx != NULL) {
            ctx->status.overflow_count++;
        }
    }
}



//
// Handlers
//

void TIM1_CC_IRQHandler(void) {
    _uni_hal_tim_irq_cc(TIM1);
}


void TIM1_UP_TIM16_IRQHandler(void) {
    if (LL_TIM_IsActiveFlag_UPDATE(TIM1)) {
        _uni_hal_tim_irq_update(TIM1);
        LL_TIM_ClearFlag_UPDATE(TIM1);
    }
}

void TIM2_IRQHandler(void) {

}

UNI_COMMON_COMPILER_WEAK void TIM7_IRQHandler(void) {
    (void)uni_hal_tim_period_elapsed(UNI_HAL_CORE_PERIPH_TIM_7);
}



//
// Functions
//

bool uni_hal_tim_init(uni_hal_tim_context_t *ctx) {
    bool result = false;
    if (ctx != NULL){
        ctx->status.inited = false;
        if(_uni_hal_tim_prescaler_valid(ctx->config.prescaler)
            && _uni_hal_tim_reloadvalue_valid(ctx->config.instance, ctx->config.reload_value))
        {
            TIM_TypeDef *handle = _uni_hal_tim_get_handle(ctx->config.instance);
            if (handle != NULL)
            {
                result = uni_hal_rcc_clk_set(ctx->config.instance, true);

                LL_TIM_InitTypeDef tim_init;
                tim_init.Prescaler = ctx->config.prescaler;
                tim_init.CounterMode = LL_TIM_COUNTERMODE_UP;
                tim_init.Autoreload = ctx->config.reload_value;
                tim_init.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
                tim_init.RepetitionCounter = 0x00;
                result = result && (LL_TIM_Init(handle, &tim_init) == SUCCESS);

                if (ctx->config.channel_count > 0 && ctx->config.channel != NULL)
                {
                    for (size_t i = 0; i < ctx->config.channel_count; i++)
                    {
                        result = result && _uni_hal_tim_init_channel(ctx, ctx->config.channel[i]);
                    }
                }

                uint32_t id = _uni_hal_tim_get_number(ctx->config.instance);
                if ((id > 0U) && (id <= UNI_HAL_TIM_MAXTIMERS))
                {
                    g_uni_hal_tim_ctx[id - 1U] = ctx;
                }
                else
                {
                    result = false;
                }
                ctx->status.inited = result;
            }
        }
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
        if (handle != NULL) {
            ctx->status.overflow_count = 0U;
            memset(ctx->status.chan, 0, sizeof(ctx->status.chan));
            LL_TIM_ClearFlag_UPDATE(handle);

            bool has_inputcapture = false;
            if (ctx->config.channel != NULL)
            {
                for (size_t i = 0; i < ctx->config.channel_count; i++)
                {
                    if (ctx->config.channel[i] != NULL && ctx->config.channel[i]->type == UNI_HAL_TIM_TYPE_INPUTCAPTURE)
                    {
                        has_inputcapture = true;
                        uni_hal_tim_channel_num_e chan_num = ctx->config.channel[i]->channel_number;

                        _uni_hal_tim_ll_capture_clear(handle, chan_num);
                        _uni_hal_tim_ll_overcapture_clear(handle, chan_num);
                        _uni_hal_tim_ll_capture_it_set(handle, chan_num, true);

                        LL_TIM_CC_EnableChannel(handle,  _uni_hal_tim_get_channel(chan_num));
                    }
                }
            }

            if (ctx->config.instance == UNI_HAL_CORE_PERIPH_TIM_1)
            {
                uni_hal_core_irq_enable(UNI_HAL_CORE_IRQ_TIM_1, 5, 0);
                if (has_inputcapture)
                {
                    LL_TIM_EnableIT_UPDATE(handle);
                    uni_hal_core_irq_enable(UNI_HAL_CORE_IRQ_TIM_1_UP, 5, 0);
                }
            }
            LL_TIM_EnableCounter(handle);
            result = true;
        }
    }

    return result;
}


bool uni_hal_tim_clear(uni_hal_tim_context_t* ctx)
{
    bool result = false;

    if (uni_hal_tim_is_inited(ctx)) {
        TIM_TypeDef *handle = _uni_hal_tim_get_handle(ctx->config.instance);
        if (handle != NULL) {
            uint32_t primask = uni_hal_core_irq_pause();

            LL_TIM_SetCounter(handle, 0U);
            LL_TIM_ClearFlag_UPDATE(handle);
            ctx->status.overflow_count = 0U;

            memset(ctx->status.chan, 0, sizeof(ctx->status.chan));

            if (ctx->config.channel != NULL)
            {
                for (size_t i = 0; i < ctx->config.channel_count; i++)
                {
                    if (ctx->config.channel[i] != NULL && ctx->config.channel[i]->type == UNI_HAL_TIM_TYPE_INPUTCAPTURE)
                    {
                        uni_hal_tim_channel_num_e chan_num = ctx->config.channel[i]->channel_number;
                        _uni_hal_tim_ll_capture_clear(handle, chan_num);
                        _uni_hal_tim_ll_overcapture_clear(handle, chan_num);
                    }
                }
            }

            uni_hal_core_irq_resume(primask);
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
            LL_TIM_SetCounter(handle, 0U);

            if (ctx->config.channel != NULL)
            {
                for (size_t i = 0; i < ctx->config.channel_count; i++)
                {
                    if (ctx->config.channel[i] != NULL && ctx->config.channel[i]->type == UNI_HAL_TIM_TYPE_INPUTCAPTURE)
                    {
                        uni_hal_tim_channel_num_e chan_num = ctx->config.channel[i]->channel_number;

                        LL_TIM_CC_DisableChannel(handle, _uni_hal_tim_get_channel(chan_num));
                        _uni_hal_tim_ll_capture_it_set(handle, chan_num, false);
                        _uni_hal_tim_ll_capture_clear(handle, chan_num);
                        _uni_hal_tim_ll_overcapture_clear(handle, chan_num);
                    }
                }
            }

            LL_TIM_DisableIT_UPDATE(handle);
            LL_TIM_ClearFlag_UPDATE(handle);
            ctx->status.overflow_count = 0U;

            memset(ctx->status.chan, 0, sizeof(ctx->status.chan));

            if (ctx->config.instance == UNI_HAL_CORE_PERIPH_TIM_1)
            {
                uni_hal_core_irq_disable(UNI_HAL_CORE_IRQ_TIM_1);
                uni_hal_core_irq_disable(UNI_HAL_CORE_IRQ_TIM_1_UP);
            }

            result = true;
        }
    }

    return result;
}

bool uni_hal_tim_period_elapsed(uni_hal_core_periph_e periph) {
    bool result = false;
    TIM_TypeDef *handle = _uni_hal_tim_get_handle(periph);

    if (handle != NULL) {
        if (LL_TIM_IsActiveFlag_UPDATE(handle)) {
            LL_TIM_ClearFlag_UPDATE(handle);
            uint32_t id = _uni_hal_tim_get_number_by_handle(handle);
            if ((id > 0U) && (id <= UNI_HAL_TIM_MAXTIMERS)) {
                uni_hal_tim_callback_fn fn = _callback_fn[id - 1];
                if (fn != NULL) {
                    result = fn(g_uni_hal_tim_ctx[id - 1], _callback_userctx[id - 1]);
                }
            }
        }
    }

    return result;
}

bool uni_hal_tim_is_channel_valid(uni_hal_tim_context_t *ctx, uni_hal_tim_channel_num_e channel)
{
    bool result = false;
    if(ctx != NULL){
        result = ((uint32_t)channel < UNI_HAL_TIM_CHANNEL_MAXCOUNT) && ctx->status.chan[channel].valid;
    }
    return result;
}


//
// get_tick_period
//

uint32_t uni_hal_tim_get_tick_period_ns(uni_hal_tim_context_t *ctx) {
    uint32_t result = 0U;

    if (uni_hal_tim_is_inited(ctx) && _uni_hal_tim_prescaler_valid(ctx->config.prescaler))
    {
        result = uni_hal_rcc_clk_get_freq(ctx->config.instance);
        result = result / (ctx->config.prescaler + 1);
        if (result)
        {
            result = 1000000000UL / result;
        }
        else
        {
            result = 0;
        }
    }

    return result;
}

uint32_t uni_hal_tim_get_tick_period_us(uni_hal_tim_context_t *ctx) {
    return uni_hal_tim_get_tick_period_ns(ctx) / 1'000;
}


bool uni_hal_tim_get_chan_state(uni_hal_tim_context_t *ctx, uni_hal_tim_channel_num_e channel, uni_hal_tim_channel_state_t *out){
    bool result = false;

    if (uni_hal_tim_is_inited(ctx) && out != NULL)
    {
        uint32_t primask = uni_hal_core_irq_pause();
        if(uni_hal_tim_is_channel_valid(ctx, channel)){
            memcpy(out, &ctx->status.chan[channel], sizeof(uni_hal_tim_channel_state_t));
            result = true;
        }
        uni_hal_core_irq_resume(primask);
    }

    return result;
}


//
// get_chan_period
//

uint64_t uni_hal_tim_get_chan_period_ns(uni_hal_tim_context_t *ctx, uni_hal_tim_channel_state_t* chan_state) {
    uint64_t result = UINT64_MAX;

    if (uni_hal_tim_is_inited(ctx) && chan_state != NULL && chan_state->valid)
    {
        result = chan_state->period * (uint64_t)uni_hal_tim_get_tick_period_ns(ctx);
    }

    return result;
}

uint64_t uni_hal_tim_get_chan_period_us(uni_hal_tim_context_t *ctx, uni_hal_tim_channel_state_t* chan_state) {
    uint64_t result = UINT64_MAX;

    uint64_t period_ns = uni_hal_tim_get_chan_period_ns(ctx, chan_state);
    if(period_ns < UINT64_MAX){
        result = period_ns / 1'000;
    }

    return result;
}


//
// get_chan_freq
//

uint64_t uni_hal_tim_get_chan_freq_mhz(uni_hal_tim_context_t *ctx, uni_hal_tim_channel_state_t* chan_state) {
    uint64_t result = UINT64_MAX;

    uint64_t period_ns = uni_hal_tim_get_chan_period_ns(ctx, chan_state);
    if (period_ns != 0U && period_ns != UINT64_MAX)
    {
        result = (1'000'000'000'000 + (period_ns / 2U)) / period_ns;
    }

    return result;
}

uint64_t uni_hal_tim_get_chan_freq_hz(uni_hal_tim_context_t* ctx, uni_hal_tim_channel_state_t* chan_state)
{
    uint64_t result = UINT64_MAX;

    uint64_t freq_mhz = uni_hal_tim_get_chan_freq_mhz(ctx, chan_state);
    if(freq_mhz != UINT64_MAX){
        result = freq_mhz / 1'000;
    }

    return result;
}

uint32_t uni_hal_tim_get_chan_age(uni_hal_tim_context_t* ctx, uni_hal_tim_channel_state_t* chan_state)
{
    uint32_t result = UINT32_MAX;
    if (uni_hal_tim_is_inited(ctx) && chan_state != NULL &&  chan_state->valid)
    {
        result = uni_hal_systick_get_ms() - chan_state->timestamp;
    }
    return result;
}

bool uni_hal_tim_set_arrpreload(uni_hal_tim_context_t* ctx, bool val)
{
    bool result = false;

    if (uni_hal_tim_is_inited(ctx))
    {
        TIM_TypeDef *handle = _uni_hal_tim_get_handle(ctx->config.instance);
        if (handle != NULL)
        {
            val ? LL_TIM_EnableARRPreload(handle) : LL_TIM_DisableARRPreload(handle);
        }
        result = true;
    }

    return result;
}

bool uni_hal_tim_set_dmarequest(uni_hal_tim_context_t * ctx, bool val)
{
    bool result = false;

    if (uni_hal_tim_is_inited(ctx))
    {
        TIM_TypeDef *handle = _uni_hal_tim_get_handle(ctx->config.instance);
        if (handle != NULL)
        {
            val ? LL_TIM_EnableDMAReq_UPDATE(handle) : LL_TIM_DisableDMAReq_UPDATE(handle);
        }
        result = true;
    }

    return result;
}
