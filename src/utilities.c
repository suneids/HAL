#include "../inc/utilities.h"
#include "../inc/gpio.h"
GPIO_Pin_t led_pin = {GPIOC, ONBOARD_LED_PIN, 0, 0, 0};
void HAL_assert(uint8_t condition){
	if(!condition){
		HAL_assertFailed();
	}
}

void HAL_assertFailed(){
	__disable_irq();//отключить периферию, прерывания

	while(1){
		GPIO_PinToggle(led_pin);
		for(volatile uint32_t i = 0; i < 500000; i++);
	}
}
