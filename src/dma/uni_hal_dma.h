#pragma once

//
// Includes
//

// stdlib
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// uni_hal
#include "core/uni_hal_core_enum.h"
#include "dma/uni_hal_dma_enum.h"
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
    #include"dma/uni_hal_dma_stm32h7.h"
#elif defined(UNI_HAL_TARGET_MCU_STM32L496)
    #include"dma/uni_hal_dma_stm32l4.h"
#endif

//
// Typedefs
//


typedef struct {
    /**
     * DMA module instance
     */
    uni_hal_core_periph_e instance;

    /**
     * DMA channel
     */
    uni_hal_dma_channel_e channel;
} uni_hal_dma_config_t;



typedef struct {
    bool initialized;
} uni_hal_dma_state_t;


/**
 * DMA Context
 */
typedef struct {
    uni_hal_dma_config_t config;

    uni_hal_dma_state_t  state;
} uni_hal_dma_context_t;



//
// Functions
//

/**
 * Initialize DMA transfer
 * @param ctx pointer to the DMA context
 * @param from pointer from
 * @param to pointer to
 * @param length transfer length
 * @return true on success
 */
bool uni_hal_dma_init(uni_hal_dma_context_t *ctx);

/** Checks that DMA context was properly initialized
 * @param ctx pointer to the DMA context
 * @return true in case of initialized DMA context
 */
bool uni_hal_dma_is_inited(const uni_hal_dma_context_t* ctx);


bool uni_hal_dma_enable(uni_hal_dma_context_t* ctx, bool val);

//
// Functions/Set
//

bool uni_hal_dma_set_direction(uni_hal_dma_context_t* ctx, uni_hal_dma_direction_e val);

bool uni_hal_dma_set_fifo_mode(uni_hal_dma_context_t* ctx, bool val);

bool uni_hal_dma_set_mode(uni_hal_dma_context_t* ctx, uni_hal_dma_mode_e val);

bool uni_hal_dma_set_priority(uni_hal_dma_context_t* ctx, uni_hal_dma_priority_e val);

bool uni_hal_dma_set_memory_align(uni_hal_dma_context_t* ctx, uni_hal_dma_alignment_e val);

bool uni_hal_dma_set_periph_align(uni_hal_dma_context_t* ctx, uni_hal_dma_alignment_e val);

bool uni_hal_dma_set_memory_increment(uni_hal_dma_context_t* ctx, uni_hal_dma_increment_e val);

bool uni_hal_dma_set_periph_increment(uni_hal_dma_context_t* ctx, uni_hal_dma_increment_e val);

bool uni_hal_dma_set_address(uni_hal_dma_context_t* ctx, uint32_t addr_from, uint32_t addr_to);

bool uni_hal_dma_set_length(uni_hal_dma_context_t* ctx, uint32_t length);

bool uni_hal_dma_set_request(uni_hal_dma_context_t* ctx);