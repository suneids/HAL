#ifndef PWM_H
#define PWM_H
#include "mcu_config.h"
#include "tim.h"
#include "gpio.h"

typedef struct {
    TIM_TypeDef *TIMx;
    uint8_t channel;
#if defined(STM32G0B1xx)
	uint8_t		af;
#endif
}TimerChannel_t;


typedef struct{
	GPIO_Pin_t  pin;
    TIM_TypeDef *TIMx;
    uint8_t     channel;
#if defined(STM32G0B1xx)
	uint8_t		af;
#endif
}PinMap_t;


extern const uint32_t CCMR_OCxM[4];
extern const uint32_t CCMR_OCxPos[4];
extern const uint32_t CCMR_OCxPE[4];
extern const uint32_t CCER_CCxE[4];

void PWM_Init(GPIO_Pin_t pin);
void PWM_Write(GPIO_Pin_t pin, uint16_t value);

TimerChannel_t TIM_GetChannel(GPIO_Pin_t pin);
#endif
