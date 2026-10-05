#include <stdio.h>
#include "../../../inc/usart.h"
#include "../../../inc/gpio.h"
#include "../../../inc/tim.h"
#if defined(STM32F103C6Tx)

static void USART_RXHandler_Default(UART_HandleTypeDef* huart, uint8_t byte);
static void USART_TXHandler_Default(UART_HandleTypeDef* huart);

volatile uint16_t rx_heads[2] = { 0 }, tx_heads[2] = { 0 };
volatile uint16_t rx_tails[2] = { 0 }, tx_tails[2] = { 0 };
volatile char usartRXBuffers[2][USART_BUFFER_SIZE] = {{ 0 }}, usartTXBuffers[2][USART_BUFFER_SIZE] = {{ 0 }};

static UART_HandleTypeDef huart1 = {USART1, NULL, USART_RXHandler_Default, USART_TXHandler_Default, 0};
static UART_HandleTypeDef huart2 = {USART2, NULL, USART_RXHandler_Default, USART_TXHandler_Default, 1};

void USART_Init(USART_TypeDef *USARTx, uint32_t  baud_rate, uint8_t parity, uint8_t stop_bits){
	if(USARTx == USART1){
		RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
		GPIO_Pin_t tx = {.port = GPIOA, .number = 9,  .mode = GPIO_MODE_OUTPUT_50MHz, .cnf = GPIO_CNF_PUSH_PULL_ALT, .pull = GPIO_PULL_NONE},
				   rx = {.port = GPIOA, .number = 10, .mode = GPIO_MODE_INPUT, .cnf = GPIO_CNF_INPUT_PU_PD, .pull = GPIO_PULL};
		GPIO_PinMode(tx);
		GPIO_PinMode(rx);
		NVIC_EnableIRQ(USART1_IRQn);
	}
	else if(USARTx == USART2){
		RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
		GPIO_Pin_t tx = {.port = GPIOA, .number = 2,  .mode = GPIO_MODE_OUTPUT_50MHz, .cnf = GPIO_CNF_PUSH_PULL_ALT, .pull = GPIO_PULL_NONE},
			       rx = {.port = GPIOA, .number = 3, .mode = GPIO_MODE_INPUT, .cnf = GPIO_CNF_FLOATING, .pull = GPIO_PULL_NONE};
		GPIO_PinMode(tx);
		GPIO_PinMode(rx);
		NVIC_EnableIRQ(USART2_IRQn);
	}
	USARTx->CR1 &= ~(USART_CR1_UE | USART_CR1_RXNEIE | USART_CR1_PCE | USART_CR1_PS | USART_CR1_M);

	volatile uint32_t tmp  = USARTx->SR;
	tmp = USARTx->DR;
	(void)tmp;

	if(!parity){
		// 8N?
		USARTx->CR1 &= ~USART_CR1_PCE;
		USARTx->CR1 &= ~USART_CR1_M;
	}else{
		//8E? 8O?
		USARTx->CR1 |= USART_CR1_PCE;
		USARTx->CR1 |= USART_CR1_M;
		if(parity == 2){
			USARTx->CR1 |= USART_CR1_PS; // Odd
		}
		else{
			USARTx->CR1 &= ~USART_CR1_PS; // Even
		}
	}

	USARTx->CR2 &= ~USART_CR2_STOP;
	if(stop_bits == 2){
		USARTx->CR2 |= (0b10 << 12);
	}

	uint32_t pclk = (USARTx == USART1) ? RCC_GetPclk2_Hz() : RCC_GetPclk1_Hz();
	USARTx->BRR = USART_BrrCalc(pclk, baud_rate);

	USARTx->CR1 |= USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;
}


uint16_t USART_Available(USART_TypeDef *USARTx){
	uint8_t port_id = USART_Index(USARTx);
	return (rx_heads[port_id] - rx_tails[port_id]) & USART_BUFFER_MASK;
}


void USART_WriteByte(USART_TypeDef *USARTx, char byte){
	uint8_t port_id = USART_Index(USARTx);
	volatile char *usartTXBuffer = usartTXBuffers[port_id];
	uint16_t next_head = (tx_heads[port_id] + 1) & USART_BUFFER_MASK;
	if(next_head == tx_tails[port_id]){
		return;
	}
	usartTXBuffer[tx_heads[port_id]] = byte;
	tx_heads[port_id] = next_head;
	USARTx->CR1 |= USART_CR1_TXEIE;
}


void USART_WriteLine(USART_TypeDef *USARTx, const char* str, uint16_t len){
	uint8_t port_id = USART_Index(USARTx);
	volatile char *usartTXBuffer = usartTXBuffers[port_id];
	for(uint16_t i = 0; i < len; i++){
		uint16_t next_head = (tx_heads[port_id] + 1) & USART_BUFFER_MASK;
		if(next_head == tx_tails[port_id]){
			return;
		}
		usartTXBuffer[tx_heads[port_id]] = *str++;
		tx_heads[port_id] = next_head;
	}
	USARTx->CR1 |= USART_CR1_TXEIE;
}


char USART_ReadByte(USART_TypeDef *USARTx){
	uint8_t port_id = USART_Index(USARTx);
	volatile char *usartRXBuffer = usartRXBuffers[port_id];
	if(rx_tails[port_id] == rx_heads[port_id]){
		return '\0';
	}
	char result = usartRXBuffer[rx_tails[port_id]];
	rx_tails[port_id] = (rx_tails[port_id] + 1) & USART_BUFFER_MASK;
	return result;
}


void USART_ReadBytes(USART_TypeDef *USARTx, char *buf, uint32_t max_len){
	uint8_t port_id = USART_Index(USARTx);
	volatile char *usartRXBuffer = usartRXBuffers[port_id];
	uint32_t current_len = 0, end = 0;
	while((current_len + 1 < max_len) && rx_tails[port_id] != rx_heads[port_id]){
		char last_byte = usartRXBuffer[rx_tails[port_id]];
		if(last_byte == '\n'){
			end = 1;
		}
		buf[current_len++] = last_byte;
		rx_tails[port_id] = (rx_tails[port_id] + 1) & USART_BUFFER_MASK;
		if(end){
			break;
		}
	}
	buf[current_len] = '\0';
}


void echo(USART_TypeDef *USARTx){
	if(USARTx->SR & USART_SR_RXNE){
		char temp = (USARTx->DR & 0xFF);
		USARTx->DR = temp;
	}
}


uint8_t USART_Index(USART_TypeDef *USARTx){
	if(USARTx == USART2){
		return 1;
	}
	return 0;
}

void USART_SetHandlers(USART_TypeDef* USARTx, UART_RxCallback customRxHandler, UART_TxCallback customTxHandler, TIM_TypeDef* timer) {
	UART_HandleTypeDef* h = (USARTx == USART1) ? &huart1 : &huart2;

	h->RxHandler = customRxHandler? customRxHandler : USART_RXHandler_Default;
	h->TxHandler = customTxHandler? customTxHandler : USART_TXHandler_Default;
	h->TimerInstance = timer; // Если нужен таймер для T3.5
}

static void USART_RXHandler_Default(UART_HandleTypeDef* huart, uint8_t byte){
	uint8_t id = huart->port_id;
	uint16_t next_head = (rx_heads[id] + 1) & USART_BUFFER_MASK;
	if(next_head != rx_tails[id]){
		volatile char *usartRXBuffer = usartRXBuffers[id];
		usartRXBuffer[rx_heads[id]] = byte;
		rx_heads[id] = next_head;
	}
}


static void USART_TXHandler_Default(UART_HandleTypeDef* huart){

}


void USART1_IRQHandler(void){
	USART_IRQHandler_Shared(&huart1);
}


void USART2_IRQHandler(void){
	USART_IRQHandler_Shared(&huart2);
}


void USART_IRQHandler_Shared(UART_HandleTypeDef *huart){
	USART_TypeDef *USARTx = huart->Instance;
	uint8_t port_id = huart->port_id;
	// 1. ==== RX (Динамическая часть) ====
	if (USARTx->SR & USART_SR_RXNE) {
		uint8_t byte = (uint8_t)(USARTx->DR & 0xFF);
		huart->RxHandler(huart, byte);
	}
	// ==== TX ====
	if ((USARTx->SR & USART_SR_TXE) && (USARTx->CR1 & USART_CR1_TXEIE)) {
		if (tx_tails[port_id] != tx_heads[port_id]) {
			USARTx->DR = usartTXBuffers[port_id][tx_tails[port_id]];
			tx_tails[port_id] = (tx_tails[port_id] + 1) & USART_BUFFER_MASK;
		}
		else{
			USARTx->CR1 &= ~USART_CR1_TXEIE;
			USARTx->CR1 |= USART_CR1_TCIE;
		}

	}

	if((USARTx->SR & USART_SR_TC) && (USARTx->CR1 & USART_CR1_TCIE)) {
		USARTx->CR1 &= ~USART_CR1_TCIE;

		// TODO: GPIO_ResetBits(DE_PIN) для Modbus
		huart->TxHandler(huart);
	}



	if(USARTx->SR & (USART_SR_ORE | USART_SR_NE | USART_SR_FE)){
		volatile uint32_t dummy = USARTx->DR;
		(void)dummy;
	}
}


uint32_t USART_BrrCalc(uint32_t pclk, uint32_t baud){
	uint32_t div16 =  (pclk + (baud / 2u)) / baud;
	uint32_t mant = div16 / 16u;
	uint32_t frac = div16 % 16u;
	if(frac > 15u){
		frac = 0;
		mant++;
	}
	return (mant << 4) | (frac & 0xFu);
}

#endif
