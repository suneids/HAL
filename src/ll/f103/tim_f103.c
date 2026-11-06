#include "../../../inc/tim.h"
#if defined(STM32F103C6Tx)

volatile uint32_t msTicks = 0;
static TimHandler_t tim_handlers[3] = {NULL, NULL, NULL};

static uint8_t TIM_GetIndex(TIM_TypeDef *TIMx){
	if(TIMx == TIM1) return 0;
	if(TIMx == TIM2) return 1;
	if(TIMx == TIM3) return 2;
	return 99;
}

void SysTick_Init(void){
	SysTick_Config(RCC_GetHclk_Hz() / 1000);
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
	}
	else

		if(TIMx == TIM2){
		RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
		if(debounce) NVIC_EnableIRQ(TIM2_IRQn);
	}
	else if(TIMx == TIM3){
		RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
//		if(debounce) NVIC_EnableIRQ(TIM3_IRQn); //Вызывает hard fault, пока не придумал решение
	}

	TIMx->PSC = psc;
	TIMx->ARR = arr;
	TIMx->CNT = 0;


	if(debounce){
		TIMx->DIER |= TIM_DIER_UIE;
		TIMx->CR1 &= ~TIM_CR1_CEN;
	}
	else{
		TIMx->DIER &= ~TIM_DIER_UIE;
		TIMx->CR1 |= TIM_CR1_ARPE;
		TIMx->EGR |= TIM_EGR_UG;
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
	if(TIM3->SR & TIM_SR_UIF){
		TIM3->SR &= ~TIM_SR_UIF;
	}

	if(tim_handlers[2] != NULL){
		tim_handlers[2]();
	}
}



uint32_t RCC_GetSysclk_Hz(void)
{
    uint32_t sws = (RCC->CFGR >> 2) & 0x3u;   // SWS: actual system clock source

    switch (sws) {
    case 0x0: // HSI
        return HSI_VALUE;

    case 0x1: // HSE
        return HSE_VALUE;

    case 0x2: { // PLL
        uint32_t pll_src_hse = (RCC->CFGR & RCC_CFGR_PLLSRC) ? 1u : 0u;
        uint32_t hse_div2    = (RCC->CFGR & RCC_CFGR_PLLXTPRE) ? 1u : 0u;

        static const uint8_t pllmul_tbl[16] = {
            2, 3, 4, 5, 6, 7, 8, 9,
            10, 11, 12, 13, 14, 15, 16, 16
        };

        uint32_t pllmul_bits = (RCC->CFGR >> 18) & 0xFu;
        uint32_t pllmul      = pllmul_tbl[pllmul_bits];

        uint32_t pll_in = pll_src_hse ? HSE_VALUE : (HSI_VALUE / 2u);
        if (pll_src_hse && hse_div2) {
            pll_in /= 2u;
        }

        return pll_in * pllmul;
    }

    default:
        return HSI_VALUE;
    }
}

uint32_t RCC_GetHclk_Hz(void)
{
    static const uint16_t ahb_presc_tbl[16] = {
        1,1,1,1,1,1,1,1, 2,4,8,16,64,128,256,512
    };
    uint32_t sys = RCC_GetSysclk_Hz();
    uint32_t hpre = (RCC->CFGR >> 4) & 0xF;
    return sys / ahb_presc_tbl[hpre];
}


uint32_t RCC_GetPclk1_Hz(void)
{
    static const uint8_t apb_presc_tbl[8] = {1,1,1,1,2,4,8,16};
    uint32_t hclk = RCC_GetHclk_Hz();
    uint32_t ppre1 = (RCC->CFGR >> 8) & 0x7;
    return hclk / apb_presc_tbl[ppre1];
}


uint32_t RCC_GetTIMPclk1_Hz(void)
{
    uint32_t pclk1 = RCC_GetPclk1_Hz();
    uint32_t ppre1 = (RCC->CFGR >> 8) & 0x7u;

    if (ppre1 >= 4u) {
        return pclk1 * 2u;
    }
    return pclk1;
}


uint32_t RCC_GetPclk2_Hz(void)
{
    static const uint8_t apb_presc_tbl[8] = {1,1,1,1,2,4,8,16};
    uint32_t hclk = RCC_GetHclk_Hz();
    uint32_t ppre2 = (RCC->CFGR >> 11) & 0x7;
    return hclk / apb_presc_tbl[ppre2];
}

static TIM_TypeDef *us_tim = 0;

void TIM_US_Init(TIM_TypeDef *TIMx)
{
    us_tim = TIMx;

#if defined(STM32F103C6Tx)
    if (TIMx == TIM2) {
        RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    }
    else if (TIMx == TIM3) {
        RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    }
//    else if (TIMx == TIM4) {
//        RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;
//    }

    uint32_t tim_clk = RCC_GetTIMPclk1_Hz();

#elif defined(STM32G0B1xx)
    if (TIMx == TIM2) {
        RCC->APBENR1 |= RCC_APBENR1_TIM2EN;
    } else if (TIMx == TIM3) {
        RCC->APBENR1 |= RCC_APBENR1_TIM3EN;
    }

    uint32_t tim_clk = RCC_GetTIMPclk1_Hz();
#endif

    /*
     * Делаем таймер 1 MHz:
     * 1 тик = 1 us
     */
    TIMx->PSC = (tim_clk / 1000000UL) - 1;
    TIMx->ARR = 0xFFFF;
    TIMx->CNT = 0;

    TIMx->CR1 |= TIM_CR1_CEN;
}

void delay_us(uint32_t us)
{
    uint16_t start = us_tim->CNT;

    while ((uint16_t)(us_tim->CNT - start) < us) {
        // wait
    }
}
#endif
