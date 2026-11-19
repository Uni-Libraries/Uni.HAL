#pragma once
#include <stdint.h>

#define XST_SUCCESS 0
#define XUARTPS_CR_OFFSET 0x00U
#define XUARTPS_IER_OFFSET 0x08U
#define XUARTPS_IDR_OFFSET 0x0CU
#define XUARTPS_IMR_OFFSET 0x10U
#define XUARTPS_ISR_OFFSET 0x14U
#define XUARTPS_SR_OFFSET 0x2CU
#define XUARTPS_FIFO_OFFSET 0x30U
#define XUARTPS_CR_RX_EN 0x04U
#define XUARTPS_CR_RX_DIS 0x08U
#define XUARTPS_CR_TX_EN 0x10U
#define XUARTPS_CR_TX_DIS 0x20U
#define XUARTPS_IXR_RXOVR 0x01U
#define XUARTPS_IXR_RXFULL 0x04U
#define XUARTPS_IXR_TXEMPTY 0x08U
#define XUARTPS_IXR_OVER 0x20U
#define XUARTPS_IXR_TOUT 0x100U
#define XUARTPS_IXR_MASK 0x3FFFU
#define XUARTPS_SR_RXEMPTY 0x02U
#define XUARTPS_SR_TXEMPTY 0x08U
#define XUARTPS_SR_TXFULL 0x10U
#define XUARTPS_SR_TACTIVE 0x800U

typedef struct { uint32_t BaseAddress; } XUartPs_Config;
typedef struct { uint32_t BaudRate; } XUartPs;
XUartPs_Config *XUartPs_LookupConfig(uint32_t base);
int XUartPs_CfgInitialize(XUartPs *dev, XUartPs_Config *cfg, uint32_t address);
int XUartPs_SetBaudRate(XUartPs *dev, uint32_t baudrate);
void XUartPs_SetFifoThreshold(XUartPs *dev, uint8_t value);
void XUartPs_SetRecvTimeout(XUartPs *dev, uint8_t value);
uint32_t XUartPs_ReadReg(uint32_t address, uint32_t offset);
void XUartPs_WriteReg(uint32_t address, uint32_t offset, uint32_t value);
