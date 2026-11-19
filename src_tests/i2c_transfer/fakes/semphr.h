#pragma once
#include "FreeRTOS.h"
typedef struct { int count; } ModelSemaphore;
typedef ModelSemaphore *SemaphoreHandle_t;
#if configSUPPORT_STATIC_ALLOCATION
typedef ModelSemaphore StaticSemaphore_t;
SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t *);
#else
SemaphoreHandle_t xSemaphoreCreateBinary(void);
#endif
void vSemaphoreDelete(SemaphoreHandle_t);
BaseType_t xSemaphoreTake(SemaphoreHandle_t, TickType_t);
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t, BaseType_t *);
