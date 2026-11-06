#include <stdio.h>
#include "../../../inc/usart.h"
#include "../../../inc/gpio.h"
#include "../../../inc/tim.h"

#if defined(STM32G474xx)

static void USART_RXHandler_Default(UART_HandleTypeDef* huart, uint8_t byte);
static void USART_TXHandler_Default(UART_HandleTypeDef* huart);

volatile uint16_t rx_heads[2] = { 0 }, tx_heads[2] = { 0 };
volatile uint16_t rx_tails[2] = { 0 }, tx_tails[2] = { 0 };
volatile char usartRXBuffers[2][USART_BUFFER_SIZE] = {{ 0 }}, usartTXBuffers[2][USART_BUFFER_SIZE] = {{ 0 }};

static UART_HandleTypeDef huart1 = {USART1, NULL, USART_RXHandler_Default, USART_TXHandler_Default, 0};
static UART_HandleTypeDef huart2 = {USART2, NULL, USART_RXHandler_Default, USART_TXHandler_Default, 1};

void USART_Init(USART_TypeDef *USARTx, uint32_t baud_rate, uint8_t parity, uint8_t stop_bits){
	if(baud_rate == 0u) return;
	GPIO_Pin_t tx = {
		.port  = GPIOA,
		.moder = GPIO_MODE_AF,
		.otype = GPIO_OTYPE_PP,
		.pull  = GPIO_NOPULL,
		.speed = GPIO_SPEED_HIGH,
		.af    = 7
	};
	GPIO_Pin_t rx = {
		.port  = GPIOA,
		.moder = GPIO_MODE_AF,
		.otype = GPIO_OTYPE_PP,
		.pull  = GPIO_PULLUP,
		.speed = GPIO_SPEED_HIGH,
		.af    = 7
	};
	uint32_t pclk;

	if(USARTx == USART1){
		/*
		 * USART1 kernel clock = PCLK2
		 * USART1SEL = 00
		 */
		RCC->CCIPR &= ~RCC_CCIPR_USART1SEL;
		RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
		tx.number = 9;
		rx.number = 10;
		pclk = RCC_GetPclk2_Hz();
		NVIC_EnableIRQ(USART1_IRQn);
	}
	else if(USARTx == USART2){
		/*
		 * USART2 kernel clock = PCLK1
		 * USART2SEL = 00
		 */
		RCC->CCIPR &= ~RCC_CCIPR_USART2SEL;
		RCC->APB1ENR1 |= RCC_APB1ENR1_USART2EN;
		tx.number = 2;
		rx.number = 3;
		pclk = RCC_GetPclk1_Hz();
		NVIC_EnableIRQ(USART2_IRQn);
	}
	else if(USARTx == USART3){

	    /*
	     * USART3 kernel clock = PCLK1
	     * USART3SEL = 00
	     * TX = PB10, RX = PB11, AF7
	     */

	    RCC->CCIPR &= ~RCC_CCIPR_USART3SEL;
	    RCC->APB1ENR1 |= RCC_APB1ENR1_USART3EN;

	    tx.port = GPIOB;
	    tx.number = 10;

	    rx.port = GPIOB;
	    rx.number = 11;

	    pclk = RCC_GetPclk1_Hz();

	    NVIC_EnableIRQ(USART3_IRQn);
	}
	else{
		return;
	}
	GPIO_PinMode(tx);
	GPIO_PinMode(rx);
	USARTx->CR1 &= ~USART_CR1_UE;
	/*
	 * Наш BRR рассчитан для:
	 *
	 * prescaler  = /1
	 * oversample = 16
	 */
	USARTx->PRESC = 0u;
	USARTx->CR1 &= ~(USART_CR1_RXNEIE_RXFNEIE | USART_CR1_PCE |
					 USART_CR1_PS | USART_CR1_M0 | USART_CR1_M1 |
					 USART_CR1_OVER8 | USART_CR1_FIFOEN);
	if(USARTx->ISR & USART_ISR_RXNE_RXFNE) (void)USARTx->RDR;
	if(USARTx->ISR & USART_ISR_ORE) USARTx->ICR = USART_ICR_ORECF;
	if(!parity){
		// 8N1
		USARTx->CR1 &= ~USART_CR1_PCE;
		USARTx->CR1 &= ~(USART_CR1_M0 | USART_CR1_M1);
	}
	else{
		// 8E1 / 8O1
		USARTx->CR1 |= USART_CR1_PCE;
		/*
		 * 9-bit frame:
		 * 8 data + parity
		 */
		USARTx->CR1 &= ~USART_CR1_M1;
		USARTx->CR1 |= USART_CR1_M0;
		if(parity == 2) USARTx->CR1 |= USART_CR1_PS;
		else 			USARTx->CR1 &= ~USART_CR1_PS;
	}
	USARTx->CR2 &= ~USART_CR2_STOP;
	if(stop_bits == 2) USARTx->CR2 |= (0b10u << USART_CR2_STOP_Pos);
	USARTx->BRR = USART_BrrCalc(pclk, baud_rate);


	USARTx->CR1 |= USART_CR1_UE | USART_CR1_TE |
				   USART_CR1_RE | USART_CR1_RXNEIE_RXFNEIE;
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
	USARTx->CR1 |= USART_CR1_TXEIE_TXFNFIE;
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
	USARTx->CR1 |= USART_CR1_TXEIE_TXFNFIE;
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
	if(USARTx->ISR & USART_ISR_RXNE_RXFNE){
		char temp = (USARTx->RDR & 0xFF);
		USARTx->TDR = temp;
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
	if (USARTx->ISR & USART_ISR_RXNE_RXFNE) {
		uint8_t byte = (uint8_t)(USARTx->RDR & 0xFF);
		huart->RxHandler(huart, byte);
	}
	// ==== TX ====
	if ((USARTx->ISR & USART_ISR_TXE_TXFNF) && (USARTx->CR1 & USART_CR1_TXEIE_TXFNFIE)) {
		if (tx_tails[port_id] != tx_heads[port_id]) {
			USARTx->TDR = usartTXBuffers[port_id][tx_tails[port_id]];
			tx_tails[port_id] = (tx_tails[port_id] + 1) & USART_BUFFER_MASK;
		}
		else{
			USARTx->CR1 &= ~USART_CR1_TXEIE_TXFNFIE;
			USARTx->CR1 |= USART_CR1_TCIE;
		}

	}

	if((USARTx->ISR & USART_ISR_TC) && (USARTx->CR1 & USART_CR1_TCIE)) {
		USARTx->CR1 &= ~USART_CR1_TCIE;

		// TODO: GPIO_ResetBits(DE_PIN) для Modbus
		huart->TxHandler(huart);
	}



	if(USARTx->ISR & USART_ISR_ORE) USARTx->ICR = USART_ICR_ORECF;
	if(USARTx->ISR &USART_ISR_NE)   USARTx->ICR = USART_ICR_NECF;
	if(USARTx->ISR & USART_ISR_FE)  USARTx->ICR = USART_ICR_FECF;
}


uint32_t USART_BrrCalc(uint32_t pclk, uint32_t baud){
	if(baud == 0u) return 0u;
	return (pclk + baud / 2u) / baud;
}

#endif
