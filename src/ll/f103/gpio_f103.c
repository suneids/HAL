#include "../../../inc/gpio.h"
#if defined(STM32F103C6Tx)


static void GPIO_ClockEnable(GPIO_Pin_t pin){
	if(pin.port == GPIOA){
		RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
	}
	else if(pin.port == GPIOB){
		RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
	}
	else if(pin.port == GPIOC){
		RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
	}
	(void)pin.port->CRL;
}


uint8_t GPIO_GetPortIndex(GPIO_TypeDef *port){
	if(port == GPIOA){
		return 0;
	}
	else if(port == GPIOB){
		return 1;
	}
	else if(port == GPIOC){
		return 2;
	}
	return 0;
}


void GPIO_PinMode(GPIO_Pin_t pin){
	GPIO_ClockEnable(pin);
	GPIO_TypeDef* GPIOx = pin.port;
	uint8_t pin_number = pin.number;
	uint8_t shift = 4*(pin_number < 8? pin_number: pin_number - 8);
	if(pin_number < 8){
		GPIOx->CRL &= ~( 0xFUL << shift);
		GPIOx->CRL |= ((pin.cnf << 2) | pin.mode) << shift;
	}
	else{
		GPIOx->CRH &= ~( 0xFUL << shift);
		GPIOx->CRH |= ((pin.cnf << 2) | pin.mode) << shift;
	}
	if(pin.mode == GPIO_MODE_INPUT && pin.cnf == GPIO_CNF_INPUT_PU_PD){
		if(pin.pull){
			GPIOx->ODR |= 1 << pin_number;
		}
		else{
			GPIOx->ODR &= ~(1 << pin_number);
		}
	}
}



void GPIO_PinModeMulti(const GPIO_Pin_t *pins, size_t pins_number){
	for(uint8_t i = 0; i < pins_number; i++)
		GPIO_PinMode(pins[i]);
}


void GPIO_PinToggle(GPIO_Pin_t pin){
	GPIO_TypeDef *GPIOx = pin.port;
	uint8_t pin_number = pin.number;
	if(GPIOx->ODR & (1U << pin_number)){
		GPIOx->BSRR = (1U << (pin_number + 16));
	}
	else{
		GPIOx->BSRR = 1U << pin_number;
	}
}


void GPIO_DigitalWrite(GPIO_Pin_t pin, uint8_t value){
	GPIO_TypeDef* GPIOx = pin.port;
	uint8_t pin_number = pin.number;
	if(value == 1){
		GPIOx->BSRR = 1 << pin_number;
	}
	else{
		GPIOx->BSRR = 1 << (pin_number+16);
	}
}


uint8_t GPIO_DigitalRead(GPIO_Pin_t pin){
	GPIO_TypeDef* GPIOx = pin.port;
	uint8_t pin_number = pin.number;
	return (GPIOx->IDR & (1 << pin_number))? 1 : 0;
}

#endif
