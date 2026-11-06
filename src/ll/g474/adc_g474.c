#include "../../../inc/adc.h"

#if defined(STM32G474xx)


static uint8_t ADC_GetChannelNumber(GPIO_Pin_t pin){

	if(pin.port == GPIOA){
		if(pin.number == 0) return 1;
		if(pin.number == 1) return 2;
		if(pin.number == 2) return 3;
		if(pin.number == 3) return 4;
	}

	if(pin.port == GPIOB){
		if(pin.number == 14) return 5;
		if(pin.number == 12) return 11;
		if(pin.number == 1)  return 12;
		if(pin.number == 11) return 14;
		if(pin.number == 0)  return 15;
	}

	if(pin.port == GPIOC){
		if(pin.number == 0) return 6;
		if(pin.number == 1) return 7;
		if(pin.number == 2) return 8;
		if(pin.number == 3) return 9;
	}

	if(pin.port == GPIOF){
		if(pin.number == 0) return 10;
	}

	return 0xFF;
}


static void ADC_WaitReady(void){
	while(!(ADC1->ISR & ADC_ISR_ADRDY)) __NOP();
}


static void ADC_DisableIfEnabled(void){

	if(ADC1->CR & ADC_CR_ADSTART){
		ADC1->CR |= ADC_CR_ADSTP;
		while(ADC1->CR & ADC_CR_ADSTP) __NOP();
	}

	if(ADC1->CR & ADC_CR_ADEN){
		ADC1->CR |= ADC_CR_ADDIS;
		while(ADC1->CR & ADC_CR_ADEN) __NOP();
	}
}


static void ADC_SetSamplingTime(uint8_t channel, uint8_t sampling){

	if(channel <= 9){

		uint32_t shift = channel * 3u;

		ADC1->SMPR1 &= ~(0x7u << shift);
		ADC1->SMPR1 |= ((uint32_t)(sampling & 0x7u) << shift);
	}
	else if(channel <= 18){

		uint32_t shift = (channel - 10u) * 3u;

		ADC1->SMPR2 &= ~(0x7u << shift);
		ADC1->SMPR2 |= ((uint32_t)(sampling & 0x7u) << shift);
	}
}


static void ADC_SetRank(uint8_t rank, uint8_t channel){

	volatile uint32_t *SQRx;
	uint32_t shift;

	if(rank >= 1 && rank <= 4){

		SQRx = &ADC1->SQR1;
		shift = 6u + ((rank - 1u) * 6u);
	}
	else if(rank >= 5 && rank <= 9){

		SQRx = &ADC1->SQR2;
		shift = (rank - 5u) * 6u;
	}
	else if(rank >= 10 && rank <= 14){

		SQRx = &ADC1->SQR3;
		shift = (rank - 10u) * 6u;
	}
	else if(rank >= 15 && rank <= 16){

		SQRx = &ADC1->SQR4;
		shift = (rank - 15u) * 6u;
	}
	else{
		return;
	}

	*SQRx &= ~(0x1Fu << shift);
	*SQRx |= ((uint32_t)(channel & 0x1Fu) << shift);
}


void ADC_InitMulti(GPIO_Pin_t *pins, uint16_t count, uint8_t need_dma){

	if(pins == NULL || count == 0 || count > 16) return;


	// Проверяем сначала все пины
	for(uint16_t i = 0; i < count; i++){

		if(ADC_GetChannelNumber(pins[i]) == 0xFF){
			return;
		}
	}


	// GPIO -> analog
	for(uint16_t i = 0; i < count; i++){

		pins[i].moder = GPIO_MODE_ANALOG;
		pins[i].otype = GPIO_OTYPE_PP;
		pins[i].pull  = GPIO_NOPULL;
		pins[i].speed = GPIO_SPEED_LOW;
		pins[i].af    = 0;

		GPIO_PinMode(pins[i]);
	}


	// ADC1 + ADC2 common clock
	RCC->AHB2ENR |= RCC_AHB2ENR_ADC12EN;


	ADC_DisableIfEnabled();


	/*
	 * ADC synchronous clock = HCLK / 4
	 *
	 * CKMODE = 11
	 */
	ADC12_COMMON->CCR &= ~(ADC_CCR_CKMODE | ADC_CCR_PRESC);
	ADC12_COMMON->CCR |= ADC_CCR_CKMODE;


	// Exit deep-power-down
	ADC1->CR &= ~ADC_CR_DEEPPWD;


	// Internal ADC regulator
	ADC1->CR |= ADC_CR_ADVREGEN;

	for(volatile uint32_t i = 0; i < 10000u; i++){
		__NOP();
	}


	// Single-ended calibration
	ADC1->CR &= ~ADC_CR_ADCALDIF;
	ADC1->CR |= ADC_CR_ADCAL;

	while(ADC1->CR & ADC_CR_ADCAL){
		__NOP();
	}


	// Basic configuration
	ADC1->CFGR = 0;

	ADC1->CFGR |= ADC_CFGR_CONT;


	if(need_dma){

		/*
		 * DMA enabled
		 * unlimited requests -> circular DMA
		 */
		ADC1->CFGR |= ADC_CFGR_DMAEN;
		ADC1->CFGR |= ADC_CFGR_DMACFG;
	}
	else{

		ADC1->CFGR &= ~ADC_CFGR_DMAEN;
		ADC1->CFGR &= ~ADC_CFGR_DMACFG;
	}


	// Clear regular sequence
	ADC1->SQR1 = 0;
	ADC1->SQR2 = 0;
	ADC1->SQR3 = 0;
	ADC1->SQR4 = 0;


	/*
	 * L = number of conversions - 1
	 */
	ADC1->SQR1 |= ((uint32_t)(count - 1u) << ADC_SQR1_L_Pos);


	for(uint16_t i = 0; i < count; i++){

		uint8_t channel = ADC_GetChannelNumber(pins[i]);

		/*
		 * sampling = 7
		 * максимальное время выборки
		 */
		ADC_SetSamplingTime(channel, 7u);

		/*
		 * rank начинается с 1
		 */
		ADC_SetRank((uint8_t)(i + 1u), channel);
	}


	// Clear status flags
	ADC1->ISR = ADC_ISR_ADRDY |
				ADC_ISR_EOC   |
				ADC_ISR_EOS   |
				ADC_ISR_OVR;


	// Enable ADC
	ADC1->CR |= ADC_CR_ADEN;

	ADC_WaitReady();
}


void ADC_Start(void){

	if(ADC1->CR & ADC_CR_ADSTART) return;

	ADC1->CR |= ADC_CR_ADSTART;
}


#endif
