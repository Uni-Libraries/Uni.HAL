// Run the shipped ST polling code on Cortex-M4, with I2C registers backed by RAM.
#include <stm32l4xx_hal.h>

typedef enum { BUS_HELD, ADDRESS_NACK, MISSING_STOP, ADDRESS_ACK } bus_response_t;

static I2C_TypeDef registers;
static unsigned starts, tick_reads;
static bus_response_t response;

uint32_t __wrap_HAL_GetTick(void)
{
    // ICR is write-one-to-clear and START is cleared by the peripheral on the bus.
    if (response == ADDRESS_NACK && (registers.ICR & I2C_ICR_STOPCF)) {
        // ST clears NACK and then STOP between two reads of the clock.
        registers.ISR &= ~I2C_ISR_NACKF;
    }
    registers.ISR &= ~registers.ICR;
    registers.ICR = 0U;
    if (registers.CR2 & I2C_CR2_START) {
        ++starts;
        registers.CR2 &= ~I2C_CR2_START;
        switch (response) {
            case ADDRESS_NACK: registers.ISR = I2C_ISR_NACKF | I2C_ISR_STOPF; break;
            case MISSING_STOP: registers.ISR = I2C_ISR_NACKF | I2C_ISR_BUSY; break;
            case ADDRESS_ACK:  registers.ISR = I2C_ISR_STOPF; break;
            case BUS_HELD: break;
        }
    }
    return (UINT32_MAX - 2U) + tick_reads++ / 8U;
}

static uint32_t check_deadlines(void)
{
    uint8_t b = 0U;
    I2C_HandleTypeDef h = {.Instance = &registers, .State = HAL_I2C_STATE_READY};
    uint32_t const timeout = 5U;
    response = BUS_HELD;
    for (unsigned op = 0; op < 4; ++op) {
        tick_reads = 0U;
        registers.ISR = I2C_ISR_BUSY | I2C_ISR_NACKF; // Busy bus; no transfer started by this call.
        HAL_StatusTypeDef s;
        switch (op) {
            case 0: s = HAL_I2C_Master_Transmit(&h, 0x80, &b, 1, timeout); break;
            case 1: s = HAL_I2C_Master_Receive(&h, 0x80, &b, 1, timeout); break;
            case 2: s = HAL_I2C_Mem_Write(&h, 0x80, 7, I2C_MEMADD_SIZE_8BIT, &b, 1, timeout); break;
            default: s = HAL_I2C_Mem_Read(&h, 0x80, 7, I2C_MEMADD_SIZE_8BIT, &b, 1, timeout); break;
        }
        if (s != HAL_ERROR || (tick_reads - 1U) / 8U != timeout || starts != 0U ||
            (registers.CR2 & I2C_CR2_STOP) || !(registers.ISR & I2C_ISR_NACKF) ||
            h.Lock != HAL_UNLOCKED || h.State != HAL_I2C_STATE_READY) {
            return __LINE__;
        }
    }

    registers.ISR = 0U;
    response = ADDRESS_NACK;
    tick_reads = 0U;
    if (HAL_I2C_IsDeviceReady(&h, 0x80, 100, timeout) != HAL_ERROR ||
        starts == 0U || starts >= 100U || (tick_reads - 1U) / 8U != timeout) {
        return __LINE__;
    }

    registers.ISR = 0U;
    response = MISSING_STOP;
    tick_reads = 0U;
    if (HAL_I2C_Master_Transmit(&h, 0x80, &b, 1, timeout) != HAL_ERROR ||
        (tick_reads - 1U) / 8U != timeout || !(registers.CR2 & I2C_CR2_STOP) ||
        !(h.ErrorCode & HAL_I2C_ERROR_TIMEOUT) || h.Lock != HAL_UNLOCKED || h.State != HAL_I2C_STATE_READY) {
        return __LINE__;
    }

    // A late NACK and STOP leave stale flags on the now idle bus. Probe a responding device.
    registers.ISR = I2C_ISR_NACKF | I2C_ISR_STOPF;
    registers.CR2 &= ~I2C_CR2_STOP;
    response = ADDRESS_ACK;
    tick_reads = starts = 0U;
    return (HAL_I2C_IsDeviceReady(&h, 0x80, 1, timeout) == HAL_OK && starts == 1U) ? 0U : __LINE__;
}

int main(void)
{
    uint32_t block[] = {0x20026U, check_deadlines()};
    register uint32_t operation __asm__("r0") = 0x20U;
    register void *args __asm__("r1") = block;
    __asm__ volatile ("bkpt 0xab" : "+r"(operation), "+r"(args) :: "memory");
    for (;;) {}
}
