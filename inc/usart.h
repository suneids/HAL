#ifndef USART_H
#define USART_H
#include <stdint.h>
#include "mcu_config.h"
#include "gpio.h"
#define USART_BUFFER_SIZE 256
#define USART_BUFFER_MASK (USART_BUFFER_SIZE - 1)


typedef struct UART_HandleTypeDef UART_HandleTypeDef;

typedef void (*UART_RxCallback)(UART_HandleTypeDef* huart, uint8_t byte);
typedef void (*UART_TxCallback)(UART_HandleTypeDef* huart);

struct UART_HandleTypeDef {
    USART_TypeDef* Instance;
    TIM_TypeDef* TimerInstance;
    UART_RxCallback RxHandler;
    UART_TxCallback TxHandler;
    uint8_t port_id;           // 0 для USART1, 1 для USART2
};


extern volatile uint16_t rx_heads[2], tx_heads[2];
extern volatile uint16_t rx_tails[2], tx_tails[2];
extern volatile char usartRXBuffers[2][USART_BUFFER_SIZE], usartTXBuffers[2][USART_BUFFER_SIZE];


void echo(USART_TypeDef *USARTx);
void USART_Init(USART_TypeDef *USARTx, uint32_t baud_rate, uint8_t parity, uint8_t stop_bits);
uint16_t USART_Available(USART_TypeDef *USARTx);
void USART_WriteByte(USART_TypeDef *USARTx, char byte);
void USART_WriteLine(USART_TypeDef *USARTx, const char *str, uint16_t len);

char USART_ReadByte(USART_TypeDef *USARTx);
void USART_ReadBytes(USART_TypeDef *USARTx, char *buf, uint32_t max_len);

void USART_SetHandlers(USART_TypeDef* USARTx, UART_RxCallback customRxHandler, UART_TxCallback customTxHandler, TIM_TypeDef* timer);
void USART_IRQHandler_Shared(UART_HandleTypeDef *huart);


uint32_t USART_BrrCalc(uint32_t pclk, uint32_t baud);
uint8_t USART_Index(USART_TypeDef *USARTx);
#endif
