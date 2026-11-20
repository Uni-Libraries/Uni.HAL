#pragma once
#include <stdint.h>
#ifndef configSUPPORT_STATIC_ALLOCATION
#define configSUPPORT_STATIC_ALLOCATION 1
#endif
typedef int BaseType_t;
typedef uint32_t TickType_t;
#define pdFALSE 0
#define pdTRUE 1
// Exercise an application-specific Cortex-M syscall ceiling.
#define configMAX_SYSCALL_INTERRUPT_PRIORITY 0x60U
#define portMAX_DELAY UINT32_MAX
#define pdMS_TO_TICKS(ms) (ms)
#define traceISR_ENTER() ((void)0)
#define portYIELD_FROM_ISR(woken) ((void)(woken))
