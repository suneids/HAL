#ifndef BUTTON_H
#define BUTTON_H
#include <stdint.h>
#include "../inc/gpio.h"
#include "../inc/tim.h"


typedef enum{
	BTN_NO_EVENT = 0,
	BTN_SHORT_CLICK,
	BTN_LONG_CLICK
} Button_Event_t;


typedef struct{
	GPIO_Pin_t pin;
	uint8_t is_pressed;
	uint32_t press_start_ms;
	uint32_t debounce_ms;
	uint32_t long_press_ms;
}Button_t;


void Button_Init(Button_t *btn, GPIO_Pin_t pin, uint32_t debounce_ms, uint32_t long_press_ms);
Button_Event_t Button_CheckPress(Button_t *btn);
#endif
