#include "button.h"

void Button_Init(Button_t *btn, GPIO_Pin_t pin, uint32_t debounce_ms, uint32_t long_press_ms){
	GPIO_PinMode(pin);
	btn->pin = pin;
	btn->debounce_ms = debounce_ms;
	btn->long_press_ms = long_press_ms;
	btn->is_pressed = !GPIO_DigitalRead(btn->pin);
	btn->press_start_ms = btn->is_pressed? millis() : 0;
}


Button_Event_t Button_CheckPress(Button_t *btn){
	Button_Event_t action = BTN_NO_EVENT;

	uint8_t btn_state = !GPIO_DigitalRead(btn->pin);

	if(btn_state && !btn->is_pressed){
		btn->press_start_ms = millis();
		btn->is_pressed = 1;
	}
	else if(!btn_state && btn->is_pressed){
		uint32_t press_duration = millis() - btn->press_start_ms;
		if(press_duration >= btn->debounce_ms){
			if(press_duration < btn->long_press_ms){
				action = BTN_SHORT_CLICK;
			}
			else{
				action = BTN_LONG_CLICK;
			}
		}
		btn->is_pressed = 0;
	}
	return action;
}

