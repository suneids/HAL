#include "../../../inc/adc.h"

#if defined(STM32F103C6Tx)
uint8_t getChannelNumber(GPIO_Pin_t pin){
	if(pin.port == GPIOA) return pin.number;
	if(pin.port == GPIOB) return pin.number + 8;
	if(pin.port == GPIOC) return pin.number + 10;
	return 0xFF;
}

void ADC_InitMulti(GPIO_Pin_t *pins, uint16_t count, uint8_t need_dma){
	for(uint16_t i = 0; i < count; i++){
		pins[i].mode = GPIO_MODE_INPUT;
		pins[i].cnf = GPIO_CNF_ANALOG;
		pins[i].pull = GPIO_PULL_NONE;
		GPIO_PinMode(pins[i]);
	}

	RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
	(void)ADC1->SR;
	RCC->CFGR &= ~(RCC_CFGR_ADCPRE_Msk);
	RCC->CFGR |= RCC_CFGR_ADCPRE_1;
	ADC1->SQR1 &= ~(ADC_SQR1_L_Msk);
	ADC1->SQR1 |= ((count - 1) << ADC_SQR1_L_Pos);
	ADC1->SQR2 = 0;
	ADC1->SQR3 = 0;
	for(uint16_t i = 0; i < count; i++){
		uint8_t channel = getChannelNumber(pins[i]);
		if(i < 6){
			ADC1->SQR3 |= (channel << (i * 5));
		}
		else if(i < 12){
			ADC1->SQR2 |= (channel << ((i - 6) * 5));
		}

	}
	ADC1->SMPR2 |= 0xFFFFFFFF;
	ADC1->SMPR1 |= 0xFFFFFFFF;
	ADC1->CR1 |= ADC_CR1_SCAN;
	// Force ADC power-cycle before calibration
	ADC1->CR2 |= ADC_CR2_CONT | ADC_CR2_ADON;
	ADC1->CR2 &= ~ADC_CR2_ADON;
	for (volatile int i = 0; i < 100; i++);
	ADC1->CR2 |= ADC_CR2_ADON;
	for (volatile int i = 0; i < 1000; i++);


	ADC1->CR2 |= ADC_CR2_RSTCAL;
	while(ADC1->CR2 & ADC_CR2_RSTCAL);
	ADC1->CR2 |= ADC_CR2_CAL;
	while(ADC1->CR2 & ADC_CR2_CAL);
	ADC1->CR2 |= (0b111 << 17) | ADC_CR2_EXTTRIG;
	if(need_dma){
			ADC1->CR2 |= ADC_CR2_DMA;
	}
	else{
		ADC1->CR2 &= ~ADC_CR2_DMA;
	}
}

void ADC_Start(){
	ADC1->CR2 |= ADC_CR2_SWSTART;
}

#endif
