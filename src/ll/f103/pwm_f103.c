#include "../../../inc/pwm.h"
#if defined(STM32F103C6Tx)

#define PWM_MAP_SIZE 12
const uint32_t CCMR_OCxM[4] = { TIM_CCMR1_OC1M, TIM_CCMR1_OC2M,
						 TIM_CCMR2_OC3M, TIM_CCMR2_OC4M };
const uint32_t CCMR_OCxPos[4] = { TIM_CCMR1_OC1M_Pos, TIM_CCMR1_OC2M_Pos,
							TIM_CCMR2_OC3M_Pos, TIM_CCMR2_OC4M_Pos };
const uint32_t CCMR_OCxPE[4] = {TIM_CCMR1_OC1PE, TIM_CCMR1_OC2PE,
						  TIM_CCMR2_OC3PE, TIM_CCMR2_OC4PE};
const uint32_t CCER_CCxE[4] = {TIM_CCER_CC1E, TIM_CCER_CC2E,
						 TIM_CCER_CC3E, TIM_CCER_CC4E };


PinMap_t map[PWM_MAP_SIZE] = {
    {{GPIOA, 8}, TIM1, 1},
    {{GPIOA, 9}, TIM1, 2},
    {{GPIOA, 10}, TIM1, 3},
    {{GPIOA, 11}, TIM1, 4},

    {{GPIOA, 0}, TIM2, 1},
    {{GPIOA, 1}, TIM2, 2},
    {{GPIOA, 2}, TIM2, 3},
    {{GPIOA, 3}, TIM2, 4},

    {{GPIOA, 6}, TIM3, 1},
    {{GPIOA, 7}, TIM3, 2},
    {{GPIOB, 0}, TIM3, 3},
	{{GPIOB, 1}, TIM3, 4},

};


void PWM_Init(GPIO_Pin_t pin){
	pin.mode = GPIO_MODE_OUTPUT_10MHz;
	pin.cnf  = GPIO_CNF_PUSH_PULL_ALT;
	pin.pull = GPIO_PULL_NONE;

	GPIO_PinMode(pin);
	TimerChannel_t pin_metadata = TIM_GetChannel(pin);
	if(pin_metadata.TIMx == NULL) return;

	TIM_TypeDef *TIMx = pin_metadata.TIMx;


	uint8_t channel = pin_metadata.channel;
	if((channel < 1) || (channel > 4)){
		//TODO assert error
	}
	__IO uint32_t *CCMRx = channel <= 2? &TIMx->CCMR1 : &TIMx->CCMR2;
	channel -= 1;
	// Определяем смещение внутри регистра CCMR (0 для CH1/CH3, 8 для CH2/CH4)
	uint8_t shift = (pin_metadata.channel % 2 == 0) ? 8 : 0;

	// 1. Сначала полностью очищаем биты режима (OCxM — 3 бита)
	*CCMRx &= ~(0x7 << (shift + 4));

	// 2. Устанавливаем PWM Mode 1 (бинарно 110, десятично 6)
	*CCMRx |= (6 << (shift + 4));

	// 3. Включаем Preload (OCxPE — 3-й бит в группе канала)
	*CCMRx |= (1 << (shift + 3));
//	*CCMRx &= ~(CCMR_OCxM[channel]);
//	*CCMRx |= (6 << CCMR_OCxPos[channel]);
	*CCMRx |= (CCMR_OCxPE[channel]);
	TIMx->CCER |= (CCER_CCxE[channel]);

	if(TIMx == TIM1) TIM1->BDTR |= TIM_BDTR_MOE;
	TIMx->EGR |= TIM_EGR_UG;
}


void PWM_Write(GPIO_Pin_t pin, uint16_t value){
	TimerChannel_t pin_metadata = TIM_GetChannel(pin);
	TIM_TypeDef *TIMx = pin_metadata.TIMx;
	uint8_t channel = pin_metadata.channel;
	__IO uint32_t *CCRs[4] = {&TIMx->CCR1, &TIMx->CCR2, &TIMx->CCR3, &TIMx->CCR4};
	if((channel > 0) && (channel < 5) && (value <= TIMx->ARR)){
		*CCRs[channel - 1] = value;
	}

}

TimerChannel_t TIM_GetChannel(GPIO_Pin_t pin){
	TimerChannel_t result = {NULL, 0};
	for(int i = 0; i < PWM_MAP_SIZE; i++){
		if((map[i].pin.port == pin.port) && ( map[i].pin.number == pin.number)){
			result.TIMx = map[i].TIMx; 
			result.channel = map[i].channel;
			return result;
		}
	}

	return result;
}
#endif
