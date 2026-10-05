#ifndef SOFT_USART_H
#define SOFT_USART_H
#define SOFT_UART_BUFFER_SIZE 32
#define SOFT_UART_BUFFER_MASK (SOFT_UART_BUFFER_SIZE - 1)
#include "mcu_config.h"
#include "gpio.h"
#include "tim.h"
#include "exti.h"

uint16_t softUartAvailable(void);
char softUartReadByte(void);
void softUartPutChar(char data);
void softUartPutString(const char *data);

void softUartInit(Pin_t rx, Pin_t tx, uint32_t baud_rate);

#endif
