#pragma once

//
// Typedefs
//

/**
 * DMA channel
 */
typedef enum {
    UNI_HAL_DMA_CHANNEL_1 = 1,
    UNI_HAL_DMA_CHANNEL_2 = 2,
    UNI_HAL_DMA_CHANNEL_3 = 3,
    UNI_HAL_DMA_CHANNEL_4 = 4,
    UNI_HAL_DMA_CHANNEL_5 = 5,
    UNI_HAL_DMA_CHANNEL_6 = 6,
    UNI_HAL_DMA_CHANNEL_7 = 7,
} uni_hal_dma_channel_e;


/**
 * DMA direction
 */
typedef enum {
    UNI_HAL_DMA_DIRECTION_P2M = 1,
    UNI_HAL_DMA_DIRECTION_M2P = 2,
    UNI_HAL_DMA_DIRECTION_M2M = 3,
} uni_hal_dma_direction_e;


/**
 *  DMA mode
 */
typedef enum {
    UNI_HAL_DMA_MODE_NORMAL,
    UNI_HAL_DMA_MODE_CIRCULAR,
    UNI_HAL_DMA_MODE_PERCTRL,
} uni_hal_dma_mode_e;


/**
 * DMA priority
 */
typedef enum {
    UNI_HAL_DMA_PRIORITY_LOW,
    UNI_HAL_DMA_PRIORITY_MEDIUM,
    UNI_HAL_DMA_PRIORITY_HIGH,
    UNI_HAL_DMA_PRIORITY_VERYHIGH
} uni_hal_dma_priority_e;


/**
 * DMA request
 */
typedef enum {
    UNI_HAL_DMA_REQUEST_0 = 0,
    UNI_HAL_DMA_REQUEST_1 = 1,
    UNI_HAL_DMA_REQUEST_2 = 2,
    UNI_HAL_DMA_REQUEST_3 = 3,
    UNI_HAL_DMA_REQUEST_4 = 4,
    UNI_HAL_DMA_REQUEST_5 = 5,
    UNI_HAL_DMA_REQUEST_6 = 6,
    UNI_HAL_DMA_REQUEST_7 = 7,
} uni_hal_dma_request_e;
