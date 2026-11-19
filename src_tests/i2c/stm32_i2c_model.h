#pragma once

#include <stdint.h>

typedef struct { uint32_t unused; } I2C_TypeDef;
extern I2C_TypeDef model_instances[4];
#define I2C1 (&model_instances[0])
#define I2C2 (&model_instances[1])
#define I2C3 (&model_instances[2])
#define I2C4 (&model_instances[3])

typedef enum { HAL_OK, HAL_BUSY, HAL_ERROR } HAL_StatusTypeDef;
enum { HAL_I2C_STATE_READY = 1, HAL_I2C_STATE_BUSY };
enum { I2C_MEMADD_SIZE_8BIT = 1, I2C_MEMADD_SIZE_16BIT = 2 };
enum { I2C_ADDRESSINGMODE_7BIT, I2C_DUALADDRESS_DISABLE, I2C_OA2_NOMASK,
       I2C_GENERALCALL_DISABLE, I2C_NOSTRETCH_DISABLE, I2C_ANALOGFILTER_ENABLE };

typedef struct I2C_HandleTypeDef {
    I2C_TypeDef *Instance;
    struct {
        uint32_t Timing, OwnAddress1, AddressingMode, DualAddressMode, OwnAddress2;
        uint32_t OwnAddress2Masks, GeneralCallMode, NoStretchMode;
    } Init;
    uint32_t State;
    uint8_t *pBuffPtr;
    uint16_t XferCount, XferSize;
    void (*XferISR)(struct I2C_HandleTypeDef *);
} I2C_HandleTypeDef;

uint32_t __get_IPSR(void);
uint32_t __get_PRIMASK(void);
uint32_t __get_BASEPRI(void);
#define __NVIC_PRIO_BITS 4U
enum { SysTick_IRQn = -1 };
uint32_t NVIC_GetPriority(int irq);
HAL_StatusTypeDef HAL_I2C_Init(I2C_HandleTypeDef *);
HAL_StatusTypeDef HAL_I2C_DeInit(I2C_HandleTypeDef *);
HAL_StatusTypeDef HAL_I2CEx_ConfigAnalogFilter(I2C_HandleTypeDef *, uint32_t);
HAL_StatusTypeDef HAL_I2CEx_ConfigDigitalFilter(I2C_HandleTypeDef *, uint32_t);
uint32_t HAL_I2C_GetState(I2C_HandleTypeDef *);
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *, uint16_t, uint32_t, uint32_t);
HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef *, uint16_t, uint8_t *, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *, uint16_t, uint8_t *, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *, uint16_t, uint16_t, uint16_t, uint8_t *, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *, uint16_t, uint16_t, uint16_t, uint8_t *, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_I2C_Mem_Read_IT(I2C_HandleTypeDef *, uint16_t, uint16_t, uint16_t, uint8_t *, uint16_t);
HAL_StatusTypeDef HAL_I2C_Mem_Write_IT(I2C_HandleTypeDef *, uint16_t, uint16_t, uint16_t, uint8_t *, uint16_t);
void HAL_I2C_EV_IRQHandler(I2C_HandleTypeDef *);
void HAL_I2C_ER_IRQHandler(I2C_HandleTypeDef *);
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *);
void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *);
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *);
