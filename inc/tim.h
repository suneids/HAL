#ifndef TIM_H
#define TIM_H
#include "mcu_config.h"
#include <stddef.h>
extern volatile uint32_t msTicks;

typedef void(*TimHandler_t)(void);


void SysTick_Init(void);
uint32_t millis(void);
void TIM_Init(TIM_TypeDef *TIMx, uint32_t psc, uint32_t arr, uint8_t debounce);
void SysTick_Delay(uint32_t ms);

void TIM_RegisterHandler(TIM_TypeDef *TIMx, TimHandler_t handler);
uint32_t RCC_GetSysclk_Hz();
uint32_t RCC_GetHclk_Hz();
uint32_t RCC_GetPclk1_Hz();
uint32_t RCC_GetPclk2_Hz();
uint32_t RCC_GetTIMPclk1_Hz();

#endif
