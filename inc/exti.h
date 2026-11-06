#ifndef EXTI_H
#define EXTI_H
#include "gpio.h"

typedef enum{
	EXTI_RISING_EDGE,
	EXTI_FALLING_EDGE,
	EXTI_BOTH_EDGES
} ExtiEdge;

typedef void (*ExtiHandler_t)(void);
void extiRegisterHandler(GPIO_Pin_t pin, ExtiHandler_t handler);

void extiInit(GPIO_Pin_t pin, ExtiEdge);
void extiClearFlag(GPIO_Pin_t pin);

__attribute__((weak)) void EXTI0_User_Handler(void);
__attribute__((weak)) void EXTI1_User_Handler(void);
__attribute__((weak)) void EXTI2_User_Handler(void);
__attribute__((weak)) void EXTI3_User_Handler(void);
__attribute__((weak)) void EXTI4_User_Handler(void);
__attribute__((weak)) void EXTI9_5_User_Handler(void);
__attribute__((weak)) void EXTI15_10_User_Handler(void);

#endif
