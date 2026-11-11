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
		case (uintptr_t)TIM6: { RCC->APBENR1 |= RCC_APBENR1_TIM6EN; break;										   }
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


void TIM3_TIM4_IRQHandler(void)
{
    if(TIM3->SR & TIM_SR_UIF)
    {
        TIM3->SR &= ~TIM_SR_UIF;

        if(tim_handlers[2] != NULL)
            tim_handlers[2]();
    }
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



void Clock64MHz_FromHSE(void){
    /*
     * HSE = 8 MHz
     *
     * PLL input = 8 MHz / M1 = 8 MHz
     * VCO       = 8 MHz * N24 = 192 MHz
     * PLLR      = 192 MHz / R3 = 64 MHz -> SYSCLK
     * PLLP      = 192 MHz / P8 = 24 MHz -> ADC source
     *
     * ADC:
     * PLLPCLK / 2 = 12 MHz
     */

    /* 1) Тактирование PWR */
    RCC->APBENR1 |= RCC_APBENR1_PWREN;
    (void)RCC->APBENR1;

    /*
     * Voltage Range 1.
     * Только Range 1 допускает SYSCLK до 64 MHz.
     */
    PWR->CR1 &= ~PWR_CR1_VOS;
    PWR->CR1 |= PWR_CR1_VOS_0;       // VOS = 01, Range 1

    while ((PWR->SR2 & PWR_SR2_VOSF) != 0U) {}

    /* 2) Включить кварцевый HSE */
    RCC->CR &= ~(RCC_CR_HSEBYP | RCC_CR_CSSON);
    RCC->CR |= RCC_CR_HSEON;

    while ((RCC->CR & RCC_CR_HSERDY) == 0U) {}

    /*
     * 3) Flash:
     * 64 MHz в Range 1 требует 2 wait states.
     */
    FLASH->ACR &= ~(FLASH_ACR_LATENCY);
    FLASH->ACR |= FLASH_ACR_LATENCY_2 | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN;

    while ((FLASH->ACR & FLASH_ACR_LATENCY) != FLASH_ACR_LATENCY_2) {}

    /* 4) Остановить PLL перед настройкой */
    RCC->CR &= ~RCC_CR_PLLON;

    while ((RCC->CR & RCC_CR_PLLRDY) != 0U) {}

    /*
     * 5) Делители шин:
     *
     * SYSCLK = 64 MHz
     * HCLK   = SYSCLK / 1 = 64 MHz
     * PCLK   = HCLK   / 1 = 64 MHz
     *
     * У G0 один APB, отдельных APB1/APB2 нет.
     * Нулевые значения HPRE и PPRE означают /1.
     */
    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE);

    /*
     * 6) PLL:
     *
     * PLLSRC = HSE
     * PLLM   = /1  -> код 0
     * PLLN   = x24 -> код 24
     * PLLP   = /8  -> код 7
     * PLLR   = /3  -> код 2
     */
    RCC->PLLCFGR = RCC_PLLCFGR_PLLSRC_HSE | (0U  << RCC_PLLCFGR_PLLM_Pos)
        | (24U << RCC_PLLCFGR_PLLN_Pos) | (7U  << RCC_PLLCFGR_PLLP_Pos)
        | RCC_PLLCFGR_PLLPEN | (2U  << RCC_PLLCFGR_PLLR_Pos) | RCC_PLLCFGR_PLLREN;

    /* 7) Включить PLL */
    RCC->CR |= RCC_CR_PLLON;

    while ((RCC->CR & RCC_CR_PLLRDY) == 0U) {}

    /* 8) Переключить SYSCLK на PLLRCLK */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLLRCLK;

    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLLRCLK) {}

    /*
     * 9) Выбрать PLLPCLK как асинхронный источник ADC.
     *
     * ADCSEL:
     * 00 = SYSCLK
     * 01 = PLLPCLK
     * 10 = HSI16
     */
    RCC->CCIPR = (RCC->CCIPR & ~RCC_CCIPR_ADCSEL) | RCC_CCIPR_ADCSEL_0;

    /*
     * Сам делитель ADC теперь находится не в RCC->CFGR,
     * а в ADC1_COMMON->CCR.
     *
     * Для записи регистров ADC его тактирование должно быть включено.
     */
    RCC->APBENR2 |= RCC_APBENR2_ADCEN;
    (void)RCC->APBENR2;

    /* Асинхронный режим ADC: источник задаётся через RCC_CCIPR */
    ADC1->CFGR2 &= ~ADC_CFGR2_CKMODE;

    /* PLLPCLK 24 MHz / 2 = ADC clock 12 MHz */
    ADC1_COMMON->CCR = (ADC1_COMMON->CCR & ~ADC_CCR_PRESC) | ADC_CCR_PRESC_0;

    /* 10) Обновить CMSIS-переменную */
    SystemCoreClockUpdate();
}


void TIM_Start(TIM_TypeDef *TIMx){
    TIMx->CR1 |= TIM_CR1_CEN;
}


void TIM_Stop(TIM_TypeDef *TIMx){
    TIMx->CR1 &= ~TIM_CR1_CEN;
}


void TIM_SetTRGOUpdate(TIM_TypeDef *TIMx){
    TIMx->CR2 = (TIMx->CR2 & ~TIM_CR2_MMS) | TIM_CR2_MMS_1;
}
#endif
