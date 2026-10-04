#pragma once

//
// The one place where the drivers meet the RTOS.
//
// Interrupt handlers and the few drivers that wait use the macros below instead of the FreeRTOS
// ones, so that the same sources also build for a main loop without an RTOS.
//

#if defined(UNI_HAL_USE_FREERTOS)

// FreeRTOS
#include <FreeRTOS.h>
#include <task.h>

/**
 * First statement of an interrupt handler
 */
#define UNI_HAL_OS_ISR_ENTER()        traceISR_ENTER()

/**
 * Last statement of an interrupt handler
 * @param woken not 0 when the handler made a task of higher priority ready
 */
#define UNI_HAL_OS_ISR_EXIT(woken)    portYIELD_FROM_ISR(woken)

/**
 * Give other tasks of the same priority a turn while polling, task context only
 */
#define UNI_HAL_OS_YIELD()            taskYIELD()

#else

// stdlib
#include <stdbool.h>

// the handlers keep their "task woken" bookkeeping, it just leads nowhere
typedef long BaseType_t;
#define pdFALSE ((BaseType_t)0)
#define pdTRUE  ((BaseType_t)1)

#define UNI_HAL_OS_ISR_ENTER()        do { } while (0)
#define UNI_HAL_OS_ISR_EXIT(woken)    do { (void)(woken); } while (0)
#define UNI_HAL_OS_YIELD()            do { } while (0)

#endif
