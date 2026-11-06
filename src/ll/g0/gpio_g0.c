#include "../../../inc/gpio.h"
#if defined(STM32G0B1xx)


static void GPIO_ClockEnable(GPIO_Pin_t pin){
	switch((uintptr_t)pin.port){
		case (uintptr_t)GPIOA: { RCC->IOPENR |= RCC_IOPENR_GPIOAEN; break; }
		case (uintptr_t)GPIOB: { RCC->IOPENR |= RCC_IOPENR_GPIOBEN; break; }
		case (uintptr_t)GPIOC: { RCC->IOPENR |= RCC_IOPENR_GPIOCEN; break; }
		case (uintptr_t)GPIOD: { RCC->IOPENR |= RCC_IOPENR_GPIODEN; break; }
		case (uintptr_t)GPIOE: { RCC->IOPENR |= RCC_IOPENR_GPIOEEN; break; }
		case (uintptr_t)GPIOF: { RCC->IOPENR |= RCC_IOPENR_GPIOFEN; break; }
		default: 		       { break; 								   }
	}
}


uint8_t GPIO_GetPortIndex(GPIO_TypeDef *port){
	uint8_t result = 0;
	switch((uintptr_t)port){
		case (uintptr_t)GPIOA: { result = 0; break; }
		case (uintptr_t)GPIOB: { result = 1; break; }
		case (uintptr_t)GPIOC: { result = 2; break; }
		case (uintptr_t)GPIOD: { result = 3; break; }
		case (uintptr_t)GPIOE: { result = 4; break; }
		case (uintptr_t)GPIOF: { result = 5; break; }
		default:  			   { break; 			}
	}
	return result;
}


void GPIO_PinMode(GPIO_Pin_t pin){
	GPIO_ClockEnable(pin);
	GPIO_TypeDef* GPIOx = pin.port;
	uint32_t shift1 = pin.number, shift2 = pin.number * 2;



	GPIOx->MODER   &= ~(0b11u << shift2);
	GPIOx->OSPEEDR &= ~(0b11u << shift2);
	GPIOx->PUPDR   &= ~(0b11u << shift2);
	GPIOx->OTYPER  &= ~(0b1u << shift1);

	GPIOx->MODER   |= ((uint32_t)(pin.moder & 0b11u) << shift2);
	GPIOx->OSPEEDR |= ((uint32_t)(pin.speed & 0b11u) << shift2);
	GPIOx->PUPDR   |= ((uint32_t)(pin.pull  & 0b11u) << shift2);
	GPIOx->OTYPER  |= ((uint32_t)(pin.otype & 0b1u ) << shift1);

	if(pin.moder == GPIO_MODE_AF){
		uint32_t idx = pin.number >> 3;
		uint32_t afr_shift = (pin.number & 0b111u) * 4u;

		GPIOx->AFR[idx] &= ~(0b1111u << afr_shift);
		GPIOx->AFR[idx] |= ((uint32_t)(pin.af & 0b1111u) << afr_shift);
	}
}

void GPIO_PinModeMulti(const GPIO_Pin_t *pins, size_t pins_number){
	for(size_t i = 0; i < pins_number; i++) GPIO_PinMode(pins[i]);
}


void GPIO_PinToggle(GPIO_Pin_t pin){
	GPIO_TypeDef *GPIOx = pin.port;
	uint8_t pin_number = pin.number;
	if(GPIOx->ODR & (1U << pin_number)) GPIOx->BRR  = 1U << pin_number;
	else								GPIOx->BSRR = 1U << pin_number;

}


void GPIO_DigitalWrite(GPIO_Pin_t pin, uint8_t value){
	GPIO_TypeDef* GPIOx = pin.port;
	if(value) GPIOx->BSRR = 1u << pin.number;
	else	  GPIOx->BRR  = 1u << pin.number;

}


uint8_t GPIO_DigitalRead(GPIO_Pin_t pin){
	GPIO_TypeDef* GPIOx = pin.port;
	uint8_t pin_number = pin.number;
	return (GPIOx->IDR & (1u << pin_number))? 1 : 0;
}

#endif
