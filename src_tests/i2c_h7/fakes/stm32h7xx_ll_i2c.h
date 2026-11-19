#pragma once
#include "stm32h7xx_hal.h"

typedef enum { SUCCESS = 0, ERROR } ErrorStatus;
ErrorStatus LL_I2C_DeInit(const I2C_TypeDef *instance);
