#include "../../../inc/tim.h"
#if defined(STM32G0B1xx)

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
	switch((uintptr_t)TIMx){
		case (uintptr_t)TIM1: { RCC->APBENR2 |= RCC_APBENR2_TIM1EN; break; 										   }
		case (uintptr_t)TIM2: { RCC->APBENR1 |= RCC_APBENR1_TIM2EN; if(debounce) NVIC_EnableIRQ(TIM2_IRQn); break; }
		case (uintptr_t)TIM3: { RCC->APBENR1 |= RCC_APBENR1_TIM3EN; break; 										   }
	}

	TIMx->PSC  = psc;
	TIMx->ARR  = arr;
	TIMx->CNT  = 0;

	TIMx->CR1  &= ~TIM_CR1_CEN;

	TIMx->CR1  |= TIM_CR1_ARPE;
	TIMx->EGR  |= TIM_EGR_UG;
	TIMx->SR   &= ~TIM_SR_UIF;

	if(debounce)  TIMx->DIER |= TIM_DIER_UIE;
	else{
		TIMx->DIER &= ~TIM_DIER_UIE;
		TIMx->CR1 |= TIM_CR1_CEN;
	}
}

// HANDLER

void TIM_RegisterHandler(TIM_TypeDef *TIMx, TimHandler_t handler){
	uint8_t index = TIM_GetIndex(TIMx);
	if(index < 3){
		tim_handlers[index] = handler;
	}
}


void TIM3_IRQHandler(void){
	if(TIM3->SR & TIM_SR_UIF)   TIM3->SR &= ~TIM_SR_UIF;
	if(tim_handlers[2] != NULL) tim_handlers[2]();

}


uint32_t RCC_GetSysclk_Hz(void)
{
    switch (RCC->CFGR & RCC_CFGR_SWS) {
		case RCC_CFGR_SWS_0: 					{ return HSE_VALUE; }
		case (RCC_CFGR_SWS_1 | RCC_CFGR_SWS_0): { return LSI_VALUE; }
		case RCC_CFGR_SWS_2:					{ return LSE_VALUE; }
		case RCC_CFGR_SWS_1:{
			uint32_t pllsource = RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC;
			uint32_t pllm 	   = ((RCC->PLLCFGR & RCC_PLLCFGR_PLLM) >> RCC_PLLCFGR_PLLM_Pos) + 1u;
			uint32_t pllmul    = ((RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> RCC_PLLCFGR_PLLN_Pos);
			uint32_t pllr 	   = (((RCC->PLLCFGR & RCC_PLLCFGR_PLLR) >> RCC_PLLCFGR_PLLR_Pos) + 1u);

			uint32_t pll_in;
			if(pllsource == 0b11u) pll_in = HSE_VALUE / pllm;
			else 				   pll_in = HSI_VALUE / pllm;
			return (pll_in * pllmul) / pllr;
		}
		case 0x00000000u:
		default:{
			uint32_t hsidiv = 1u << ((READ_BIT(RCC->CR, RCC_CR_HSIDIV)) >> RCC_CR_HSIDIV_Pos);
			return HSI_VALUE / hsidiv;
		}
	}
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


uint32_t RCC_GetPclk1_Hz(void)
{
    static const uint8_t apb_shift_tbl[8] = {
    		0, 0, 0, 0,
			1, 2, 3, 4
    };

    uint32_t hclk = RCC_GetHclk_Hz();
    uint32_t ppre = (RCC->CFGR & RCC_CFGR_PPRE) >> RCC_CFGR_PPRE_Pos;

    return hclk >> apb_shift_tbl[ppre];
}


uint32_t RCC_GetTIMPclk1_Hz(void)
{
    uint32_t pclk = RCC_GetPclk1_Hz();
    uint32_t ppre = (RCC->CFGR & RCC_CFGR_PPRE) >> RCC_CFGR_PPRE_Pos;

    if(ppre >= 0b100u) pclk *= 2u;

    return pclk;
}


uint32_t RCC_GetPclk2_Hz(void)
{
    return RCC_GetPclk1_Hz();
}

#endif
