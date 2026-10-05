//
// Includes
//

// uni_common
#include <uni_common.h>

// uni_hal
#include "can/uni_hal_can.h"
#include "core/uni_hal_core.h"



//
// Functions
//

uint32_t uni_hal_can_is_available(const uni_hal_can_context_t *ctx) {
    uint32_t result = 0;
    if (ctx != NULL) {
#if defined(UNI_HAL_CAN_USE_FREERTOS)
        result = uxQueueMessagesWaiting(ctx->status.queue_rx);
#else
        result = uni_common_ringbuffer_length(ctx->config.buffer_rx);
#endif
    }

    return result;
}


bool uni_hal_can_is_inited(const uni_hal_can_context_t *ctx) {
    bool result = false;
    if (ctx != NULL) {
        result = ctx->status.inited;
    }
    return result;
}


bool uni_hal_can_receive(uni_hal_can_context_t *ctx, uni_hal_can_msg_t *msg, size_t timeout_ms) {
    bool result = false;

    if (uni_hal_can_is_inited(ctx) && msg != NULL) {
#if defined(UNI_HAL_CAN_USE_FREERTOS)
        result = xQueueReceive(ctx->status.queue_rx, msg, timeout_ms == portMAX_DELAY ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms)) == pdPASS;
#else
        // the receive buffer is polled: without an RTOS there is nothing to wait on
        (void)timeout_ms;
        result = uni_common_ringbuffer_pop(ctx->config.buffer_rx, (uint8_t *)msg, 1U);
#endif
    }

    return result;
}


bool uni_hal_can_set_error_callback(uni_hal_can_context_t *ctx, uni_hal_can_error_callback_t callback, void *cookie) {
    bool result = false;
    if (uni_hal_can_is_inited(ctx)) {
        // the interrupts use both values: change them together
        uint32_t const primask = uni_hal_core_irq_pause();
        ctx->status.error_callback_cookie = cookie;
        ctx->status.error_callback = callback;
        uni_hal_core_irq_resume(primask);
        result = true;
    }
    return result;
}


bool uni_hal_can_timing_calc(uint32_t clock_hz, uint32_t bitrate, uni_hal_can_timing_t *timing) {
    if (timing == NULL || bitrate == 0U || clock_hz == 0U || (clock_hz % bitrate) != 0U) {
        return false;
    }

    // clock / bitrate = prescaler * quanta per bit. Among the exact solutions take the one whose
    // sample point is closest to 87.5 %, and of those the one with the most quanta.
    uint32_t const ticks_per_bit = clock_hz / bitrate;
    bool found = false;
    uint32_t best_error = UINT32_MAX;

    for (uint32_t quanta = 25U; quanta >= 8U; quanta--) {
        if ((ticks_per_bit % quanta) != 0U) {
            continue;
        }
        uint32_t const prescaler = ticks_per_bit / quanta;
        if (prescaler == 0U || prescaler > 1024U) {
            continue;
        }

        // sample point = (1 + bs1) / quanta, in 1/1000
        uint32_t bs1 = ((quanta * 875U) + 500U) / 1000U - 1U;
        if (bs1 > 16U) {
            bs1 = 16U;
        }
        uint32_t bs2 = quanta - 1U - bs1;
        if (bs2 < 1U) {
            bs2 = 1U;
            bs1 = quanta - 2U;
        }
        if (bs2 > 8U || bs1 < 1U || bs1 > 16U) {
            continue;
        }

        uint32_t const sample_point = ((1U + bs1) * 1000U) / quanta;
        uint32_t const error = (sample_point > 875U) ? (sample_point - 875U) : (875U - sample_point);
        if (error < best_error) {
            best_error = error;
            timing->prescaler = prescaler;
            timing->bs1 = bs1;
            timing->bs2 = bs2;
            timing->sjw = 1U;
            found = true;
        }
    }

    return found;
}
