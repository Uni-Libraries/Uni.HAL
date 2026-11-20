#pragma once

// Real FDCAN clock selection and bit timing with the RCC, CAN registers and message RAM in RAM.
#include <stm32h7xx.h>
#include "can/uni_hal_can_internal.h"

static RCC_TypeDef rcc;
static FDCAN_GlobalTypeDef controllers[2];
static uint32_t ram[2560];
#undef RCC_BASE
#define RCC_BASE ((uintptr_t)&rcc)
#undef FDCAN1
#undef FDCAN2
#define FDCAN1 (&controllers[0])
#define FDCAN2 (&controllers[1])
#undef SRAMCAN_BASE
#define SRAMCAN_BASE ((uintptr_t)ram)

#include "../../3rdparty/st_stm32h7/src_hal/stm32h7xx_ll_rcc.c"
#include "rcc/uni_hal_rcc_stm32h7.c"

static uni_hal_can_context_t *contender;
static bool contender_inited;
static bool init_pin(uni_hal_gpio_pin_context_t *pin) {
    if (contender != nullptr) {
        uni_hal_can_context_t *other = contender;
        contender = nullptr;
        contender_inited = uni_hal_can_init(other);
    }
    pin->inited = true;
    return true;
}
#define uni_hal_gpio_pin_init init_pin

static QueueHandle_t init_queue(QueueHandle_t *queue) {
    static StaticQueue_t storage;
    *queue = (QueueHandle_t)&storage;
    return *queue;
}
#define uni_hal_can_init_rx_queue init_queue
// Exercise a board that reserves interrupt priorities 0..5 for non-kernel users.
#include <FreeRTOS.h>
#if configMAX_SYSCALL_INTERRUPT_PRIORITY < (6U << (8U - __NVIC_PRIO_BITS))
#undef configMAX_SYSCALL_INTERRUPT_PRIORITY
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (6U << (8U - __NVIC_PRIO_BITS))
#endif
#include "can/uni_hal_can_stm32h7.c"
