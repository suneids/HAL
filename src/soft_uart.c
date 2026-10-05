//#include "../inc/soft_uart.h"
//#define SOFT_UART_TIM TIM3
//#define SOFT_UART_BITS_PER_FRAME 10 // 1 start + 8 data + 1 stop
//
//static void softUartRxStartHandler(void);
//static void softUartTimDispatch(void);
//static void softUartStartTx(void);
//
//typedef struct {
//    uint8_t rx_byte;
//    uint8_t bit_count;
//    uint32_t arr_value;
//} SoftUartRxState_t;
//
//typedef struct {
//    uint8_t tx_byte;
//    uint8_t bit_count;
//    uint8_t tx_busy;
//} SoftUartTxState_t;
//
//volatile char soft_uart_rx_buffer[SOFT_UART_BUFFER_SIZE],
//              soft_uart_tx_buffer[SOFT_UART_BUFFER_SIZE];
//volatile uint16_t rx_tail = 0, rx_head = 0,
//                  tx_tail = 0, tx_head = 0;
//static SoftUartRxState_t rx_state = {0};
//static SoftUartTxState_t tx_state = {0};
//Pin_t soft_uart_rx_pin,
//      soft_uart_tx_pin;
//
//void softUartInit(Pin_t rx, Pin_t tx, uint32_t baud_rate){
//	GPIO_PinMode(tx, GPIO_MODE_OUTPUT_50MHz, GPIO_CNF_PUSH_PULL, 0);
//	GPIO_PinMode(rx, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PU_PD, 1);
//    uint32_t soft_uart_arr = SYS_CLK_HZ / baud_rate;
//    rx_state.arr_value = soft_uart_arr;
//    TIM_Init(SOFT_UART_TIM, 0, soft_uart_arr, 1);
//    extiInit(rx, EXTI_FALLING_EDGE);
//    soft_uart_rx_pin = rx;
//    soft_uart_tx_pin = tx;
//
//    extiRegisterHandler(rx, softUartRxStartHandler);
//    TIM_RegisterHandler(SOFT_UART_TIM, softUartTimDispatch);
//
//    soft_uart_tx_pin.port->BSRR = (1 << soft_uart_tx_pin.number);
//    TIM3->CR1 |= TIM_CR1_CEN;
//	NVIC_EnableIRQ(TIM3_IRQn);
//}
//
//
//static void softUartRxStartHandler(void){
//    EXTI->IMR &= ~(1 << soft_uart_rx_pin.number);
//
//    rx_state.bit_count = 0;
//    rx_state.rx_byte = 0;
//
//    uint32_t sample_time = SOFT_UART_TIM->CNT + (rx_state.arr_value * 3 / 2);
//
//    SOFT_UART_TIM->CCR1  = (uint16_t)(sample_time % (SOFT_UART_TIM->ARR + 1));
//
//    SOFT_UART_TIM->SR &= ~TIM_SR_CC1IF;
//    SOFT_UART_TIM->DIER |= TIM_DIER_CC1IE;
//}
//
//
//static void softUartRxTimHandler(void){
//	uint8_t current_bit = (soft_uart_rx_pin.port->IDR & (1 << soft_uart_rx_pin.number)) ? 1 : 0;
//
//    rx_state.bit_count++;
//
//    if(rx_state.bit_count <=8){
//        uint8_t shift_amount = rx_state.bit_count - 1;
//        uint8_t bit_mask = (1 << shift_amount);
//        if(current_bit){
//            rx_state.rx_byte |= bit_mask;
//        }
//        else{
//            rx_state.rx_byte &= ~bit_mask;
//        }
//        uint32_t next_sample = SOFT_UART_TIM->CCR1 + rx_state.arr_value;
//        SOFT_UART_TIM->CCR1 = (uint16_t)(next_sample %(SOFT_UART_TIM->ARR + 1));
//    }
//    else{
//    	SOFT_UART_TIM->DIER &= ~TIM_DIER_CC1IE;
//    	if(current_bit == 1){
//    		uint16_t next_head = (rx_head + 1) & SOFT_UART_BUFFER_MASK;
//    		if(next_head != rx_tail){
//                soft_uart_rx_buffer[rx_head] = rx_state.rx_byte;
//                rx_head = next_head;
//    		}
//    	}
//    	EXTI->PR = (1 << soft_uart_rx_pin.number);
//        EXTI->IMR |= (1 << soft_uart_rx_pin.number);
//        rx_state.bit_count = 0;
//    }
//}
//
//static inline void txSetHigh(void){
//    soft_uart_tx_pin.port->BSRR = (1 << soft_uart_tx_pin.number);
//}
//
//
//static inline void txSetLow(void){
//    soft_uart_tx_pin.port->BRR = (1 << soft_uart_tx_pin.number);
//}
//
//
//static void softUartTxTimHandler(void){
//    if(tx_state.bit_count == 0){
//        txSetLow();
//    }
//    else if(tx_state.bit_count >= 1 && tx_state.bit_count <=8){
//        uint8_t bit_index = tx_state.bit_count - 1;
//        if(tx_state.tx_byte & (1 << bit_index)){
//            txSetHigh();
//        }
//        else{
//            txSetLow();
//        }
//    }
//    else if(tx_state.bit_count == 9){
//        txSetHigh();
//    }
//    else if(tx_state.bit_count == 10){
////        SOFT_UART_TIM->CR1 &= ~TIM_CR1_CEN;
//        tx_state.tx_busy = 0;
//        if(tx_head != tx_tail){
//            softUartStartTx();
//        }
//    }
//    tx_state.bit_count++;
//}
//
//
//static void softUartStartTx(void){
//    if(tx_head == tx_tail) return;
//
//    tx_state.tx_byte = soft_uart_tx_buffer[tx_tail];
//    tx_tail = (tx_tail + 1) & SOFT_UART_BUFFER_MASK;
//
//    tx_state.bit_count = 0;
//    tx_state.tx_busy = 1;
//}
//
//
//void softUartPutChar(char data){
//    __disable_irq();
//	uint16_t next_head = (tx_head + 1) & SOFT_UART_BUFFER_MASK;
//
//    if(next_head != tx_tail){
//        soft_uart_tx_buffer[tx_head] = data;
//        tx_head = next_head;
//    }
//
//    if(!tx_state.tx_busy){
//        softUartStartTx();
//    }
//    __enable_irq();
//}
//
//void softUartPutString(const char *data){
//	while(*data){
//		softUartPutChar(*data);
//		data++;
//	}
//}
//
//
//static void softUartTimDispatch(void){
//    if(SOFT_UART_TIM->SR & TIM_SR_CC1IF){
//    	SOFT_UART_TIM->SR &= ~TIM_SR_CC1IF;
//    	if((EXTI->IMR & (1 << soft_uart_rx_pin.number)) == 0){
//    		softUartRxTimHandler();
//    	}
//    }
//    if(tx_state.tx_busy){
//        softUartTxTimHandler();
//    }
//    else if(tx_head != tx_tail){
//        softUartStartTx();
//    }
//}
//
//
//uint16_t softUartAvailable(void){
//    return (rx_head - rx_tail) & SOFT_UART_BUFFER_MASK;
//}
//
//
//char softUartReadByte(void){
//    if(rx_head == rx_tail){
//        return 0;
//    }
//    char result = soft_uart_rx_buffer[rx_tail];
//    rx_tail = (rx_tail + 1) & SOFT_UART_BUFFER_MASK;
//    return result;
//}
