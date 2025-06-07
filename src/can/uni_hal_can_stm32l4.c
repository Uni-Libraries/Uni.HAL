//
// Includes
//

// stdlib
#include <stdio.h>

// st
#include <stm32l496xx.h>
#include <stm32l4xx_hal.h>
#include <stm32l4xx_hal_can.h>

// uni_hal
#include "can/uni_hal_can.h"
#include "rcc/uni_hal_rcc.h"

//
// Defines
//

#define UNI_HAL_CAN_QUEUE_SIZE (32U)



//
// Context Storage
//

static CAN_HandleTypeDef _uni_hal_can_1_handle = {};
static CAN_HandleTypeDef _uni_hal_can_2_handle = {};

static uni_hal_can_context_t *_uni_hal_can_1_ctx = NULL;
static uni_hal_can_context_t *_uni_hal_can_2_ctx = NULL;

static BaseType_t _uni_hal_can_irq_wake = false;


//
// Interrupts
//

void CAN1_RX0_IRQHandler(void)
{
#if defined(UNI_HAL_CAN_USE_FREERTOS)
    traceISR_ENTER();
    _uni_hal_can_irq_wake = pdFALSE;
#endif

    HAL_CAN_IRQHandler(&_uni_hal_can_1_handle);

#if defined(UNI_HAL_CAN_USE_FREERTOS)
    portYIELD_FROM_ISR( _uni_hal_can_irq_wake );
#endif
}

void CAN1_RX1_IRQHandler(void)
{
#if defined(UNI_HAL_CAN_USE_FREERTOS)
    traceISR_ENTER();
    _uni_hal_can_irq_wake = pdFALSE;
#endif

    HAL_CAN_IRQHandler(&_uni_hal_can_1_handle);

#if defined(UNI_HAL_CAN_USE_FREERTOS)
    portYIELD_FROM_ISR( _uni_hal_can_irq_wake );
#endif
}

void CAN2_RX0_IRQHandler(void)
{
#if defined(UNI_HAL_CAN_USE_FREERTOS)
    traceISR_ENTER();
    _uni_hal_can_irq_wake = pdFALSE;
#endif

    HAL_CAN_IRQHandler(&_uni_hal_can_2_handle);

#if defined(UNI_HAL_CAN_USE_FREERTOS)
    portYIELD_FROM_ISR( _uni_hal_can_irq_wake );
#endif
}

void CAN2_RX1_IRQHandler(void)
{
#if defined(UNI_HAL_CAN_USE_FREERTOS)
    traceISR_ENTER();
    _uni_hal_can_irq_wake = pdFALSE;
#endif

    HAL_CAN_IRQHandler(&_uni_hal_can_2_handle);

#if defined(UNI_HAL_CAN_USE_FREERTOS)
    portYIELD_FROM_ISR( _uni_hal_can_irq_wake );
#endif
}


//
// Private functions
//

/**
 * Enable CAN interrupts
 * @param instance target CAN instance
 * @param priority interrupt priority
 */
bool _uni_hal_can_interrupt_enable(uni_hal_core_periph_e instance, uint32_t priority) {
    bool result = false;
    switch (instance) {
    case UNI_HAL_CORE_PERIPH_CAN_1:
        NVIC_SetPriority(CAN1_RX0_IRQn, priority);
        NVIC_EnableIRQ(CAN1_RX0_IRQn);

        NVIC_SetPriority(CAN1_RX1_IRQn, priority);
        NVIC_EnableIRQ(CAN1_RX1_IRQn);
        result = true;
        break;
    case UNI_HAL_CORE_PERIPH_CAN_2:
        NVIC_SetPriority(CAN2_RX0_IRQn, priority);
        NVIC_EnableIRQ(CAN2_RX0_IRQn);

        NVIC_SetPriority(CAN2_RX1_IRQn, priority);
        NVIC_EnableIRQ(CAN2_RX1_IRQn);
        result = true;
        break;
    default:
        break;
    }
    return result;
}

uni_hal_can_context_t *_uni_hal_can_get_context(CAN_HandleTypeDef *hcan) { //-V2009
    uni_hal_can_context_t *result = NULL;

    if (hcan != NULL) {
        if (hcan->Instance == CAN1) {
            result = _uni_hal_can_1_ctx;
        } else if (hcan->Instance == CAN2) {
            result = _uni_hal_can_2_ctx;
        }
    }

    return result;
}

bool _uni_hal_can_set_context(uni_hal_can_context_t *ctx)
{
    bool result = false;
    switch (ctx->config.instance) {
    case UNI_HAL_CORE_PERIPH_CAN_1:
        _uni_hal_can_1_ctx = ctx;
        result = true;
        break;
    case UNI_HAL_CORE_PERIPH_CAN_2:
        _uni_hal_can_2_ctx = ctx;
        result = true;
        break;
    default:
        break;
    }

    return result;
}


CAN_TypeDef *_uni_hal_can_get_handle(uni_hal_core_periph_e instance) {
    CAN_TypeDef *result = NULL;
    switch (instance) {
    case UNI_HAL_CORE_PERIPH_CAN_1:
        result = CAN1;
        break;
    case UNI_HAL_CORE_PERIPH_CAN_2:
        result = CAN2;
        break;
    default:
        break;
    }

    return result;
}


CAN_HandleTypeDef *_uni_hal_can_get_handle_hal(uni_hal_core_periph_e instance) {
    CAN_HandleTypeDef *result = NULL;
    switch (instance) {
    case UNI_HAL_CORE_PERIPH_CAN_1:
        result = &_uni_hal_can_1_handle;
        break;
    case UNI_HAL_CORE_PERIPH_CAN_2:
        result = &_uni_hal_can_2_handle;
        break;
    default:
        break;
    }

    return result;
}


//
// Functions
//

bool uni_hal_can_init(uni_hal_can_context_t *ctx) {
    bool result = false;
    if (ctx != NULL) {
        result = _uni_hal_can_set_context(ctx);

#if defined(UNI_HAL_CAN_USE_FREERTOS)
        ctx->status.queue_rx = xQueueCreate(UNI_HAL_CAN_QUEUE_SIZE, sizeof(uni_hal_can_msg_t));
        result = result && ctx->status.queue_rx != NULL;
#else
        result =
            result && uni_common_ringbuffer_init(ctx->config.buffer_rx, ctx->config.buffer_rx->data, ctx->config.buffer_rx->size_object, ctx->config.buffer_rx->size_total);
#endif

        result = result && uni_hal_rcc_clk_set(ctx->config.instance, true);
        result = result && uni_hal_gpio_pin_init(ctx->config.pin_rx);
        result = result && uni_hal_gpio_pin_init(ctx->config.pin_tx);
        result = result && _uni_hal_can_interrupt_enable(ctx->config.instance, 4U);

        CAN_HandleTypeDef *instance_hal = _uni_hal_can_get_handle_hal(ctx->config.instance);
        CAN_TypeDef *instance = _uni_hal_can_get_handle(ctx->config.instance);
        if (result && instance != NULL && instance_hal != NULL) {
            instance_hal->Instance = instance;
            instance_hal->Init.Prescaler = 10;
            instance_hal->Init.Mode = CAN_MODE_NORMAL;
            instance_hal->Init.SyncJumpWidth = CAN_SJW_1TQ;
            instance_hal->Init.TimeSeg1 = CAN_BS1_8TQ;
            instance_hal->Init.TimeSeg2 = CAN_BS2_1TQ;
            instance_hal->Init.TimeTriggeredMode = DISABLE;
            instance_hal->Init.AutoBusOff = DISABLE;
            instance_hal->Init.AutoWakeUp = DISABLE;
            instance_hal->Init.AutoRetransmission = DISABLE;
            instance_hal->Init.ReceiveFifoLocked = DISABLE;
            instance_hal->Init.TransmitFifoPriority = DISABLE;
            result = HAL_CAN_Init(instance_hal) == HAL_OK;
            ctx->status.count_rx = 0U;
            ctx->status.count_tx = 0U;
            ctx->status.count_err = 0U;
            ctx->status.inited = result;
        }
    }

    return result;
}

bool uni_hal_can_start(uni_hal_can_context_t *ctx) { //-V2009
    bool result = false;
    if (uni_hal_can_is_inited(ctx)) {
        result = HAL_CAN_Start(_uni_hal_can_get_handle_hal(ctx->config.instance)) == HAL_OK;
    }

    return result;
}

bool uni_hal_can_stop(uni_hal_can_context_t *ctx) { //-V2009
    bool result = false;
    if (uni_hal_can_is_inited(ctx)) {
        result = HAL_CAN_Stop(_uni_hal_can_get_handle_hal(ctx->config.instance)) == HAL_OK;
    }

    return result;
}

bool uni_hal_can_set_filter(uni_hal_can_context_t *ctx, uint32_t fifo_num, uint32_t slot_idx, uint32_t filter_id, //-V2009
                           uint32_t filter_mask) {
    bool result = false;

    if (uni_hal_can_is_inited(ctx) && fifo_num < 2)
    {
        void* handle = _uni_hal_can_get_handle_hal(ctx->config.instance);
        HAL_CAN_ActivateNotification(handle, fifo_num == 1 ? CAN_IT_RX_FIFO1_MSG_PENDING : CAN_IT_RX_FIFO0_MSG_PENDING);

        CAN_FilterTypeDef rx_can_flt = {
            .FilterIdHigh = filter_id >> 16,
            .FilterIdLow = filter_id & UINT16_MAX,
            .FilterMaskIdHigh = filter_mask >> 16,
            .FilterMaskIdLow = filter_mask & UINT16_MAX,
            .FilterFIFOAssignment = fifo_num == 1 ? CAN_FILTER_FIFO1 : CAN_FILTER_FIFO0,
            .FilterBank = slot_idx, // TODO: clamp
            .FilterMode = CAN_FILTERMODE_IDMASK,
            .FilterScale = CAN_FILTERSCALE_32BIT,
            .FilterActivation = CAN_FILTER_ENABLE,
            .SlaveStartFilterBank = 14
        };

        result = HAL_CAN_ConfigFilter(handle, &rx_can_flt) == HAL_OK;
    }

    return result;
}


bool uni_hal_can_transmit(uni_hal_can_context_t *ctx, uni_hal_can_msg_t *msg) {
    bool result = false;

    if (uni_hal_can_is_inited(ctx) && msg != NULL) {
        CAN_TxHeaderTypeDef tx_msg_header = {.StdId = 0,
                .ExtId = msg->id,
                .IDE = CAN_ID_EXT,
                .RTR = CAN_RTR_DATA,
                .DLC = msg->dlc,
                .TransmitGlobalTime = DISABLE};
        uint32_t tx_mailbox = 0;

        CAN_HandleTypeDef* instance = _uni_hal_can_get_handle_hal(ctx->config.instance);
        if(instance != NULL) {
            HAL_CAN_AddTxMessage(instance, &tx_msg_header, msg->data, &tx_mailbox);
            while (HAL_CAN_IsTxMessagePending(instance, tx_mailbox)) {
            } // TODO: implement timeout
            ctx->status.count_tx++;
        }

        result = true;
    }

    return result;
}

//
// HAL Callbacks
//

static void _uni_hal_can_callback_msgpending(uni_hal_can_context_t *ctx, uint32_t fifo) {
    (void)fifo; // TODO: support two separate queues for different FIFOs?

    if (uni_hal_can_is_inited(ctx)) {
        CAN_RxHeaderTypeDef rx_header;
        uni_hal_can_msg_t msg;

        while (HAL_CAN_GetRxMessage(_uni_hal_can_get_handle_hal(ctx->config.instance), fifo, &rx_header, msg.data) == HAL_OK) {
            msg.id = rx_header.IDE ? rx_header.ExtId : rx_header.StdId;
            msg.dlc = rx_header.DLC;

#if defined(UNI_HAL_CAN_USE_FREERTOS)
            xQueueSendFromISR(ctx->status.queue_rx, &msg, &_uni_hal_can_irq_wake);
#else
            uni_common_ringbuffer_push(ctx->config.buffer_rx, (uint8_t *)&msg, 1U);
#endif

            ctx->status.count_rx++;
        }
    }
}

static void _uni_hal_can_callback_error(uni_hal_can_context_t *ctx) {
    if (ctx != NULL) {
        ctx->status.count_err++;
    }
}


void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    _uni_hal_can_callback_msgpending(_uni_hal_can_get_context(hcan), CAN_RX_FIFO0);
}


void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    _uni_hal_can_callback_msgpending(_uni_hal_can_get_context(hcan), CAN_RX_FIFO1);
}


void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan) { _uni_hal_can_callback_error(_uni_hal_can_get_context(hcan)); }
