#pragma once

#include "dma/uni_hal_dma.h"

// Caller masks IRQs while acquiring, programming or releasing a stream.
typedef void (*uni_hal_dma_stm32h7_event_fn)(void *owner, const uni_hal_dma_context_t *dma, bool error);
bool uni_hal_dma_stm32h7_claim(const uni_hal_dma_context_t *dma, uni_hal_dma_stm32h7_event_fn event, void *owner);
void uni_hal_dma_stm32h7_circular_started(const uni_hal_dma_context_t *dma);
// Returns any transfer error still pending when the disabled stream is released.
bool uni_hal_dma_stm32h7_release(const uni_hal_dma_context_t *dma);
