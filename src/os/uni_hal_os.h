#pragma once

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
