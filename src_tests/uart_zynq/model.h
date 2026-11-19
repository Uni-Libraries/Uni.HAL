#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <FreeRTOS.h>

void uart_model_rx(unsigned port, uint8_t byte);
void uart_model_irq(unsigned port);
unsigned uart_model_tx_count(unsigned port);
uint8_t uart_model_tx_byte(unsigned port, unsigned index);
bool uart_model_in_isr(void);
void uart_model_task_enter(void);
void uart_model_task_exit(void);
void uart_model_yield(BaseType_t wake);
extern unsigned uart_model_bad_task_critical, uart_model_yields;
