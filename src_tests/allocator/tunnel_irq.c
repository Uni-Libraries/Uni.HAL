#include <string.h>
#include <stm32l4xx.h>
#include "io/uni_hal_io_tunnel.h"

static uni_hal_io_tunnel_context_t tunnel;
static uint8_t block[] = {1, 2, 3, 4, 5, 6, 7, 8};
static volatile int transmit_result;

void WWDG_IRQHandler(void) {
    transmit_result = uni_hal_io_tunnel_transmit(&tunnel, tunnel.io_first, block, sizeof block);
}

static int forward_from_irq(void) {
    transmit_result = -1;
    NVIC_SetPendingIRQ(WWDG_IRQn);
    __DSB();
    __ISB();
    return transmit_result;
}

int uni_hal_test_tunnel_irq(void) {
    UNI_HAL_IO_DEFINITION(source, 2U, 2U);
    UNI_HAL_IO_DEFINITION(peer, 2U, 9U);
    uint8_t queued[] = {'x', 'x', 'x', 'x', 'x'};
    uint8_t output[sizeof block];
    if (!uni_hal_io_init(&source_ctx) || !uni_hal_io_init(&peer_ctx) ||
        !uni_hal_io_tunnel_init(&tunnel, &source_ctx, &peer_ctx, false) ||
        uni_hal_io_transmit_data(&peer_ctx, queued, sizeof queued) != sizeof queued) {
        return __LINE__;
    }
    NVIC_SetPriority(WWDG_IRQn, configMAX_SYSCALL_INTERRUPT_PRIORITY >> (8U - __NVIC_PRIO_BITS));
    NVIC_EnableIRQ(WWDG_IRQn);
    if (forward_from_irq() != UNI_HAL_IO_TUNNEL_FAIL ||
        xStreamBufferReceive(peer_ctx.buf_tx.handle, output, sizeof output, 0U) != sizeof queued ||
        memcmp(output, queued, sizeof queued) != 0) {
        return __LINE__;
    }
    if (forward_from_irq() != UNI_HAL_IO_TUNNEL_OK ||
        xStreamBufferReceive(peer_ctx.buf_tx.handle, output, sizeof output, 0U) != sizeof block ||
        memcmp(output, block, sizeof block) != 0) {
        return __LINE__;
    }
    NVIC_DisableIRQ(WWDG_IRQn);
    uni_hal_io_tunnel_init(&tunnel, nullptr, nullptr, false);
    uni_hal_io_deinit(&peer_ctx);
    uni_hal_io_deinit(&source_ctx);
    return 0;
}
