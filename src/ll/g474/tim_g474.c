#include "../../../inc/tim.h"
#if defined(STM32G474xx)

volatile uint32_t msTicks = 0;
static TimHandler_t tim_handlers[3] = {NULL, NULL, NULL};

static uint16_t TIM_GetIndex(TIM_TypeDef *TIMx){
	switch((uintptr_t)TIMx){
		case (uintptr_t)TIM1: { return 0; break; }
		case (uintptr_t)TIM2: { return 1; break; }
		case (uintptr_t)TIM3: { return 2; break; }
	}
	return 99;
}

void SysTick_Init(void){
	uint32_t hclk = RCC_GetHclk_Hz();

	if(hclk == 0) {
		while(1); // clock error
	}
	SysTick_Config(hclk / 1000);
}


uint32_t millis(void){
	return msTicks;
}


void SysTick_Handler(void){
	msTicks++;
}


void SysTick_Delay(uint32_t ms){
	uint32_t start = millis();
	while((millis() - start) < ms){
		__NOP();
	}
}


void TIM_Init(TIM_TypeDef *TIMx, uint32_t psc, uint32_t arr, uint8_t debounce){

	if(TIMx == TIM1){
		RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
		if(debounce) NVIC_EnableIRQ(TIM1_UP_TIM16_IRQn);
	}
	else if(TIMx == TIM2){
		RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
		if(debounce) NVIC_EnableIRQ(TIM2_IRQn);
	}
	else if(TIMx == TIM3){
		RCC->APB1ENR1 |= RCC_APB1ENR1_TIM3EN;
		if(debounce) NVIC_EnableIRQ(TIM3_IRQn);
	}
	else if(TIMx == TIM4){
	    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM4EN;
	    if(debounce) NVIC_EnableIRQ(TIM4_IRQn);

	}
	else if(TIMx == TIM5){
	    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM5EN;
	    if(debounce) NVIC_EnableIRQ(TIM5_IRQn);

	}
	else if(TIMx == TIM15){
	    RCC->APB2ENR |= RCC_APB2ENR_TIM15EN;
	    if(debounce) NVIC_EnableIRQ(TIM1_BRK_TIM15_IRQn);

	}
	else if(TIMx == TIM17){
	    RCC->APB2ENR |= RCC_APB2ENR_TIM17EN;
	    if(debounce) NVIC_EnableIRQ(TIM1_TRG_COM_TIM17_IRQn);
	}
	else{

	    return;

	}


	TIMx->CR1 &= ~TIM_CR1_CEN;
	TIMx->PSC = psc;
	TIMx->ARR = arr;
	TIMx->CNT = 0;
	TIMx->CR1 |= TIM_CR1_ARPE;
	TIMx->EGR = TIM_EGR_UG;
	TIMx->SR &= ~TIM_SR_UIF;

	if(debounce) TIMx->DIER |= TIM_DIER_UIE;
	else{
		TIMx->DIER &= ~TIM_DIER_UIE;
		TIMx->CR1 |= TIM_CR1_CEN;
	}
}

// HANDLER

void TIM_RegisterHandler(TIM_TypeDef *TIMx, TimHandler_t handler){
	uint8_t index = TIM_GetIndex(TIMx);
	if(index < 3) tim_handlers[index] = handler;

}


uint32_t RCC_GetSysclk_Hz(void){

	uint32_t sws = RCC->CFGR & RCC_CFGR_SWS;


	if(sws == RCC_CFGR_SWS_HSI) return HSI_VALUE;
	if(sws == RCC_CFGR_SWS_HSE) return HSE_VALUE;
	if(sws == RCC_CFGR_SWS_PLL){

		uint32_t pllcfgr = RCC->PLLCFGR;
		uint32_t source = pllcfgr & RCC_PLLCFGR_PLLSRC;
		uint32_t pllm = ((pllcfgr & RCC_PLLCFGR_PLLM) >> RCC_PLLCFGR_PLLM_Pos) + 1u;
		uint32_t plln = (pllcfgr & RCC_PLLCFGR_PLLN) >> RCC_PLLCFGR_PLLN_Pos;
		uint32_t pllr = (((pllcfgr & RCC_PLLCFGR_PLLR) >> RCC_PLLCFGR_PLLR_Pos) + 1u) * 2u;
		uint32_t source_hz;

		if(source == RCC_PLLCFGR_PLLSRC_HSE) 	  source_hz = HSE_VALUE;
		else if(source == RCC_PLLCFGR_PLLSRC_HSI) source_hz = HSI_VALUE;
		else 									  return 0u;

		uint64_t vco = ((uint64_t)source_hz * plln) / pllm;

		return (uint32_t)(vco / pllr);
	}

	return 0u;
}


uint32_t RCC_GetHclk_Hz(void)
{
    static const uint16_t ahb_shift_tbl[16] = {
    		0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9
    };
    uint32_t sys = RCC_GetSysclk_Hz();
    uint32_t hpre = (RCC->CFGR & RCC_CFGR_HPRE) >> RCC_CFGR_HPRE_Pos;
    return sys >> ahb_shift_tbl[hpre];
}


uint32_t RCC_GetPclk1_Hz(void){
	static const uint8_t apb_shift_tbl[8] = {
		0, 0, 0, 0,
		1, 2, 3, 4
	};
	uint32_t hclk = RCC_GetHclk_Hz();
	uint32_t ppre = (RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;
	return hclk >> apb_shift_tbl[ppre];
}


uint32_t RCC_GetTIMPclk1_Hz(void){
	uint32_t pclk = RCC_GetPclk1_Hz();
	uint32_t ppre = (RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;
	if(ppre >= 0b100u) pclk *= 2u;
	return pclk;
}


uint32_t RCC_GetTIMPclk2_Hz(void){
	uint32_t pclk = RCC_GetPclk2_Hz();
	uint32_t ppre = (RCC->CFGR & RCC_CFGR_PPRE2) >> RCC_CFGR_PPRE2_Pos;

	if(ppre >= 0b100u) pclk *= 2u;

	return pclk;
}


uint32_t RCC_GetPclk2_Hz(void){
	static const uint8_t apb_shift_tbl[8] = {
		0, 0, 0, 0,
		1, 2, 3, 4
	};

	uint32_t hclk = RCC_GetHclk_Hz();

	uint32_t ppre = (RCC->CFGR & RCC_CFGR_PPRE2) >> RCC_CFGR_PPRE2_Pos;

	return hclk >> apb_shift_tbl[ppre];
}

void TIM1_UP_TIM16_IRQHandler(void){
	if(TIM1->SR & TIM_SR_UIF){
		TIM1->SR &= ~TIM_SR_UIF;
		if(tim_handlers[0] != NULL) tim_handlers[0]();
	}
}


void TIM2_IRQHandler(void){
	if(TIM2->SR & TIM_SR_UIF){
		TIM2->SR &= ~TIM_SR_UIF;
		if(tim_handlers[1] != NULL) tim_handlers[1]();
	}
}


void TIM3_IRQHandler(void){
	if(TIM3->SR & TIM_SR_UIF){
		TIM3->SR &= ~TIM_SR_UIF;
		if(tim_handlers[2] != NULL) tim_handlers[2]();
	}
}

#endif
