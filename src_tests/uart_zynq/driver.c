// The PC port has no nested UART interrupts. Enter through the registered GIC vector in the model.
#include <FreeRTOS.h>
#include <task.h>
#include "model.h"
#include "zynq_core.h"
#undef taskENTER_CRITICAL
#undef taskEXIT_CRITICAL
#undef taskENTER_CRITICAL_FROM_ISR
#undef taskEXIT_CRITICAL_FROM_ISR
#undef portYIELD_FROM_ISR
#define taskENTER_CRITICAL() uart_model_task_enter()
#define taskEXIT_CRITICAL() uart_model_task_exit()
#define taskENTER_CRITICAL_FROM_ISR() ((UBaseType_t)0)
#define taskEXIT_CRITICAL_FROM_ISR(mask) ((void)(mask))
#define portYIELD_FROM_ISR(woken) uart_model_yield(woken)
#define xPortIsInsideInterrupt() uart_model_in_isr()
#include "uart/uni_hal_uart_zynq7000.c"
