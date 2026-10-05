#ifndef UTILITIES_H_
#define UTILITIES_H_
#include "mcu_config.h"
#define ONBOARD_LED_PIN 13
void HAL_assert(uint8_t condition);
void HAL_assertFailed();

#endif /* UTILITIES_H_ */
