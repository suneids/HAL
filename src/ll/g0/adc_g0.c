#include "../../../inc/adc.h"

#if defined(STM32G0B1xx)

uint8_t getChannelNumber(GPIO_Pin_t pin){
	if(pin.port == GPIOA) return pin.number;
	if(pin.port == GPIOB) return pin.number + 8;
	if(pin.port == GPIOC) return pin.number + 10;
	return 0xFF;
}


static void ADC_WaitReady(void){
	while(!(ADC1->ISR & ADC_ISR_ADRDY)) __NOP();
}


static void ADC_DisableIfEnabled(void){
	if(ADC1->CR & ADC_CR_ADEN){
		ADC1->CR |= ADC_CR_ADDIS;
		while(ADC1->CR & ADC_CR_ADEN) __NOP();
	}
}


void ADC_InitMulti(GPIO_Pin_t *pins, uint16_t count, uint8_t need_dma){
	if(pins == 0 || count == 0) return;
	for(uint16_t i = 0; i < count; i++){
		pins[i].moder = GPIO_MODE_ANALOG;
		pins[i].otype = GPIO_OTYPE_PP;
		pins[i].pull  = GPIO_NOPULL;
		pins[i].speed = GPIO_SPEED_LOW;
		pins[i].af	  = 0;
		GPIO_PinMode(pins[i]);
	}

	RCC->APBENR2 |= RCC_APBENR2_ADCEN;

	ADC_DisableIfEnabled();

#ifdef ADC_CR_DEEPPWD
	ADC1->CR &= ~ADC_CR_DEEPPWD;
#endif

#ifdef ADC_CR_ADVREGEN
	ADC1->CR |= ADC_CR_ADVREGEN;
	for(volatile int i = 0; i < 10000; i++) __NOP();
#endif

	ADC1->CR |= ADC_CR_ADCAL;
	while(ADC1->CR & ADC_CR_ADCAL) __NOP();

	ADC1->CFGR1 = 0;

	ADC1->CFGR1 |= ADC_CFGR1_CONT;
	if(need_dma){
		ADC1->CFGR1 |= ADC_CFGR1_DMAEN;
		ADC1->CFGR1 &= ~ADC_CFGR1_SCANDIR;

#ifdef ADC_CFGR1_DMACFG
		ADC1->CFGR1 |= ADC_CFGR1_DMACFG;
#endif
	}
	else{
		ADC1->CFGR1 &= ~ADC_CFGR1_DMAEN;
#ifdef ADC_CFGR1_DMACFG
		ADC1->CFGR1 &= ~ADC_CFGR1_DMACFG;
#endif
	}

#ifdef ADC_SMPR_SMP1_Msk
	ADC1->SMPR &= ~ADC_SMPR_SMP1_Msk;
	ADC1->SMPR |= (7u << ADC_SMPR_SMP1_Pos);
#else
	ADC1->SMPR = 7u;
#endif


	ADC1->CHSELR = 0;
	for(uint16_t i = 0; i < count; i++){
		uint8_t channel = getChannelNumber(pins[i]);
		if(channel <= 18) ADC1->CHSELR |= (1u << channel);
	}


	ADC1->ISR = ADC_ISR_ADRDY | ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR;
	ADC1->CR  |= ADC_CR_ADEN;
	ADC_WaitReady();
}

void ADC_Start(){
	if(ADC1->CR & ADC_CR_ADSTART) return;

	ADC1->CR |= ADC_CR_ADSTART;
}

#endif
