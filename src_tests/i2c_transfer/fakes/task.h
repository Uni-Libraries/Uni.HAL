#pragma once
#include "FreeRTOS.h"
enum { taskSCHEDULER_RUNNING = 2 };
int xTaskGetSchedulerState(void);
void vTaskDelay(TickType_t ticks);
