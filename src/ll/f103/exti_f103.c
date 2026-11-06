#include "../../../inc/exti.h"
#if defined(STM32F103C6Tx)

static ExtiHandler_t exti_handlers[16] = {NULL};


void extiInit(GPIO_Pin_t pin, ExtiEdge edge){
	GPIO_TypeDef *GPIOx = pin.port;
	uint8_t pin_number = pin.number;
	RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
	uint8_t index = pin_number/4;
	uint8_t offset = (pin_number%4)*4;
	AFIO->EXTICR[index] &= ~(0xF << offset);
	AFIO->EXTICR[index] |= (GPIO_GetPortIndex(GPIOx) << offset);

	EXTI->IMR |= (1 << pin_number);
	if(edge == EXTI_RISING_EDGE){
		EXTI->RTSR |= (1 << pin_number);
	}
	else if(edge == EXTI_FALLING_EDGE){
		EXTI->FTSR |= (1 << pin_number);
	}
	else if(edge == EXTI_BOTH_EDGES){
		EXTI->RTSR |= (1 << pin_number);
		EXTI->FTSR |= (1 << pin_number);
	}

	if(pin_number <= 4){
		NVIC_EnableIRQ(EXTI0_IRQn + pin_number);
	}
	else if(pin_number <= 9){
		NVIC_EnableIRQ(EXTI9_5_IRQn);
	}
	else{
		NVIC_EnableIRQ(EXTI15_10_IRQn);
	}
}


void extiClearFlag(GPIO_Pin_t pin){
	uint8_t pin_number = pin.number;
	EXTI->PR |= (1 << pin_number);
}

// HANDLER

void extiRegisterHandler(GPIO_Pin_t pin, ExtiHandler_t handler){
	uint8_t pin_number = pin.number;
	if(pin_number < 16){
		exti_handlers[pin_number] = handler;
	}
}


static void extiGroupDispatch(uint8_t start_number, uint8_t end_number){
	for(uint8_t number = start_number; number <= end_number; number++){
		if((EXTI->PR & (1 << number)) && (EXTI->IMR & (1 << number))){
			if(exti_handlers[number] != NULL){
				exti_handlers[number]();
			}
			EXTI->PR = (1 << number);
		}
	}
}


void EXTI0_IRQHandler(void){
	extiGroupDispatch(0, 0);
	if(EXTI0_User_Handler) EXTI0_User_Handler();
}


void EXTI1_IRQHandler(void){
	extiGroupDispatch(1, 1);
	if(EXTI1_User_Handler) EXTI1_User_Handler();
}


void EXTI2_IRQHandler(void){
	extiGroupDispatch(2, 2);
	if(EXTI2_User_Handler) EXTI2_User_Handler();
}


void EXTI3_IRQHandler(void){
	extiGroupDispatch(3, 3);
	if(EXTI3_User_Handler) EXTI3_User_Handler();
}


 void EXTI4_IRQHandler(void){
	extiGroupDispatch(4, 4);
	if(EXTI4_User_Handler) EXTI4_User_Handler();
}


void EXTI9_5_IRQHandler(void){
	extiGroupDispatch(5, 9);
	if(EXTI9_5_User_Handler) EXTI9_5_User_Handler();
}


void EXTI15_10_IRQHandler(void){
	extiGroupDispatch(10, 15);
	if(EXTI15_10_User_Handler) EXTI15_10_User_Handler();
}

#endif
