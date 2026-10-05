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


// data bytes of the CAN FD data length codes 9..15
static const uint8_t g_uni_hal_can_fd_lengths[] = {12U, 16U, 20U, 24U, 32U, 48U, 64U};


uint8_t uni_hal_can_dlc_from_length(uint32_t length, bool fd) {
    uint8_t result = UINT8_MAX;

    if (length <= 8U) {
        result = (uint8_t)length;
    }
    else if (fd) {
        for (uint32_t idx = 0U; idx < sizeof(g_uni_hal_can_fd_lengths); idx++) {
            if (g_uni_hal_can_fd_lengths[idx] == length) {
                result = (uint8_t)(9U + idx);
                break;
            }
        }
    }

    return result;
}


uint8_t uni_hal_can_dlc_to_length(uint32_t dlc, bool fd) {
    uint8_t result;

    if (dlc <= 8U) {
        result = (uint8_t)dlc;
    }
    else if (fd && dlc <= 15U) {
        result = g_uni_hal_can_fd_lengths[dlc - 9U];
    }
    else {
        result = 8U;
    }

    return result;
}


/**
 * What a bit timing has to fit into and aim for
 */
typedef struct {
    /** time quanta per bit that are tried */
    uint32_t quanta_min;
    uint32_t quanta_max;
    /** largest values the peripheral takes */
    uint32_t prescaler_max;
    uint32_t bs1_max;
    uint32_t bs2_max;
    /** wanted sample point in 1/1000 of the bit time */
    uint32_t sample_point;
    /** largest synchronisation jump width; it is made as wide as bs2 allows up to this */
    uint32_t sjw_max;
} uni_hal_can_timing_limits_t;

static bool _uni_hal_can_timing_search(uint32_t clock_hz, uint32_t bitrate, const uni_hal_can_timing_limits_t *limits,
                                       uni_hal_can_timing_t *timing) {
    if (timing == NULL || bitrate == 0U || clock_hz == 0U || (clock_hz % bitrate) != 0U) {
        return false;
    }

    // clock / bitrate = prescaler * quanta per bit. Among the exact solutions take the one whose
    // sample point is closest to the wanted one, and of those the one with the most quanta.
    uint32_t const ticks_per_bit = clock_hz / bitrate;
    bool found = false;
    uint32_t best_error = UINT32_MAX;

    for (uint32_t quanta = limits->quanta_max; quanta >= limits->quanta_min; quanta--) {
        if ((ticks_per_bit % quanta) != 0U) {
            continue;
        }
        uint32_t const prescaler = ticks_per_bit / quanta;
        if (prescaler == 0U || prescaler > limits->prescaler_max) {
            continue;
        }

        // sample point = (1 + bs1) / quanta, in 1/1000
        uint32_t bs1 = ((quanta * limits->sample_point) + 500U) / 1000U - 1U;
        if (bs1 > limits->bs1_max) {
            bs1 = limits->bs1_max;
        }
        uint32_t bs2 = quanta - 1U - bs1;
        if (bs2 < 1U) {
            bs2 = 1U;
            bs1 = quanta - 2U;
        }
        if (bs2 > limits->bs2_max || bs1 < 1U || bs1 > limits->bs1_max) {
            continue;
        }

        uint32_t const sample_point = ((1U + bs1) * 1000U) / quanta;
        uint32_t const error = (sample_point > limits->sample_point) ? (sample_point - limits->sample_point)
                                                                     : (limits->sample_point - sample_point);
        if (error < best_error) {
            best_error = error;
            timing->prescaler = prescaler;
            timing->bs1 = bs1;
            timing->bs2 = bs2;
            timing->sjw = (bs2 < limits->sjw_max) ? bs2 : limits->sjw_max;
            found = true;
        }
    }

    return found;
}


bool uni_hal_can_timing_calc(uint32_t clock_hz, uint32_t bitrate, uni_hal_can_timing_t *timing) {
    // within the ranges of bxCAN, which FDCAN covers as well
    static const uni_hal_can_timing_limits_t limits = {
        .quanta_min = 8U, .quanta_max = 25U, .prescaler_max = 1024U, .bs1_max = 16U, .bs2_max = 8U,
        .sample_point = 875U, .sjw_max = 1U,
    };
    return _uni_hal_can_timing_search(clock_hz, bitrate, &limits, timing);
}


bool uni_hal_can_timing_calc_data(uint32_t clock_hz, uint32_t bitrate, uni_hal_can_timing_t *timing) {
    // The data phase of CAN FD runs at up to 8 Mbit/s, where a bit has few time quanta. Its
    // sample point sits earlier than in the arbitration phase, and the jump width is as wide as
    // the timing allows, to follow the transmitter at these rates.
    static const uni_hal_can_timing_limits_t limits = {
        .quanta_min = 5U, .quanta_max = 25U, .prescaler_max = 32U, .bs1_max = 32U, .bs2_max = 16U,
        .sample_point = 800U, .sjw_max = 16U,
    };
    return _uni_hal_can_timing_search(clock_hz, bitrate, &limits, timing);
}
