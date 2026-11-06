#include "../../../inc/pwm.h"
#if defined(STM32G474xx)


const uint32_t CCER_CCxE[4] =   { TIM_CCER_CC1E, TIM_CCER_CC2E,
						 	      TIM_CCER_CC3E, TIM_CCER_CC4E };

const uint32_t CCMR_OCxM[4]   = { TIM_CCMR1_OC1M, TIM_CCMR1_OC2M,
						 	 	  TIM_CCMR2_OC3M, TIM_CCMR2_OC4M };
const uint32_t CCMR_OCxPos[4] = { TIM_CCMR1_OC1M_Pos, TIM_CCMR1_OC2M_Pos,
								  TIM_CCMR2_OC3M_Pos, TIM_CCMR2_OC4M_Pos };
const uint32_t CCMR_OCxPE[4]  = { TIM_CCMR1_OC1PE, TIM_CCMR1_OC2PE,
								  TIM_CCMR2_OC3PE, TIM_CCMR2_OC4PE};

static const uint32_t CCMR_CCxS[4]   = { TIM_CCMR1_CC1S, TIM_CCMR1_CC2S,
							      	  	 TIM_CCMR2_CC3S, TIM_CCMR2_CC4S };
static const PinMap_t map[] = {
    // TIM1
    {{GPIOA, 8},  TIM1, 1, 6},
    {{GPIOA, 9},  TIM1, 2, 6},
    {{GPIOA, 10}, TIM1, 3, 6},
    {{GPIOA, 11}, TIM1, 4, 11},

    // TIM2
    {{GPIOA, 0}, TIM2, 1, 1},
    {{GPIOA, 1}, TIM2, 2, 1},
    {{GPIOA, 2}, TIM2, 3, 1},

    // TIM3
    {{GPIOA, 6}, TIM3, 1, 2},
    {{GPIOB, 0}, TIM3, 3, 2},
    {{GPIOB, 1}, TIM3, 4, 2},

    // TIM4
    {{GPIOB, 6}, TIM4, 1, 2},
    {{GPIOA, 12}, TIM4, 2, 10},
    {{GPIOB, 8}, TIM4, 3, 2},
    {{GPIOB, 9}, TIM4, 4, 2},

    // TIM5
    {{GPIOA, 3}, TIM5, 4, 2},

    // TIM15
    {{GPIOB, 14}, TIM15, 1, 1},
    {{GPIOB, 15}, TIM15, 2, 1},

    // TIM17
    {{GPIOA, 7}, TIM17, 1, 1},
};

#define PWM_MAP_SIZE (sizeof(map) / sizeof(map[0]))

static void PWM_TimerClockEnable(TIM_TypeDef *TIMx){
	if(TIMx == TIM1)  RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
	if(TIMx == TIM2)  RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
	if(TIMx == TIM3)  RCC->APB1ENR1 |= RCC_APB1ENR1_TIM3EN;
	if(TIMx == TIM4)  RCC->APB1ENR1 |= RCC_APB1ENR1_TIM4EN;
	if(TIMx == TIM5)  RCC->APB1ENR1 |= RCC_APB1ENR1_TIM5EN;
	if(TIMx == TIM15) RCC->APB2ENR |= RCC_APB2ENR_TIM15EN;
	if(TIMx == TIM17) RCC->APB2ENR |= RCC_APB2ENR_TIM17EN;
}


void PWM_Init(GPIO_Pin_t pin){

	TimerChannel_t pin_metadata = TIM_GetChannel(pin);
	if(pin_metadata.TIMx == NULL) return;

	TIM_TypeDef *TIMx = pin_metadata.TIMx;
	uint8_t channel = pin_metadata.channel;

	if((channel < 1) || (channel > 4)) return; //TODO assert error

	PWM_TimerClockEnable(TIMx);

	pin.moder = GPIO_MODE_AF;
	pin.otype = GPIO_OTYPE_PP;
	pin.speed = GPIO_SPEED_HIGH;
	pin.pull  = GPIO_PULLDOWN;
	pin.af = pin_metadata.af;

	GPIO_PinMode(pin);



	__IO uint32_t *CCMRx = channel <= 2? &TIMx->CCMR1 : &TIMx->CCMR2;
	channel -= 1;
	*CCMRx &= ~(CCMR_CCxS[channel]);
	*CCMRx &= ~(CCMR_OCxM[channel]);
	*CCMRx |= (6 << CCMR_OCxPos[channel]);
	*CCMRx |= (CCMR_OCxPE[channel]);
	TIMx->CCER |= (CCER_CCxE[channel]);

	if(TIMx == TIM1 || TIMx == TIM15 || TIMx == TIM17) TIMx->BDTR |= TIM_BDTR_MOE;

	TIMx->EGR |= TIM_EGR_UG;
}


void PWM_Write(GPIO_Pin_t pin, uint16_t value){
	TimerChannel_t pin_metadata = TIM_GetChannel(pin);
	if(pin_metadata.TIMx == NULL) return;
	TIM_TypeDef *TIMx = pin_metadata.TIMx;
	uint8_t channel = pin_metadata.channel;
	__IO uint32_t *CCRs[4] = {&TIMx->CCR1, &TIMx->CCR2, &TIMx->CCR3, &TIMx->CCR4};
	if(value > TIMx->ARR) value = TIMx->ARR;

	if((channel > 0) && (channel < 5)){
		*CCRs[channel - 1] = value;
	}

}

TimerChannel_t TIM_GetChannel(GPIO_Pin_t pin){
	TimerChannel_t result = {NULL, 0, 0};
	for(size_t i = 0; i < PWM_MAP_SIZE; i++){
		if((map[i].pin.port == pin.port) && ( map[i].pin.number == pin.number)){
			result.TIMx = map[i].TIMx; 
			result.channel = map[i].channel;
			result.af	   = map[i].af;
			return result;
		}
	}

	return result;
}
#endif
