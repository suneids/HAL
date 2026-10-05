#ifndef GPIO_H
#define GPIO_H
#include "mcu_config.h"
#include <stddef.h>

#if defined(STM32F103C6Tx)
	#define GPIO_MODE_INPUT 0b00
	#define GPIO_MODE_OUTPUT_10MHz 0b01
	#define GPIO_MODE_OUTPUT_2MHz 0b10
	#define GPIO_MODE_OUTPUT_50MHz 0b11

	#define GPIO_CNF_ANALOG 0b00
	#define GPIO_CNF_FLOATING 0b01
	#define GPIO_CNF_INPUT_PU_PD 0b10


	#define GPIO_CNF_PUSH_PULL 0b00
	#define GPIO_CNF_OPEN_DRAIN 0b01
	#define GPIO_CNF_PUSH_PULL_ALT 0b10
	#define GPIO_CNF_OPEN_DRAIN_ALT 0b11

	#define GPIO_PULL 0b01
	#define GPIO_PULL_NONE 0b00

#elif defined(STM32G0B1xx)
	#define GPIO_MODE_INPUT   0U
	#define GPIO_MODE_OUTPUT  1U
	#define GPIO_MODE_AF      2U
	#define GPIO_MODE_ANALOG  3U

	#define GPIO_OTYPE_PP     0U
	#define GPIO_OTYPE_OD     1U

	#define GPIO_NOPULL       0U
	#define GPIO_PULLUP       1U
	#define GPIO_PULLDOWN     2U

	#define GPIO_SPEED_LOW        0U
	#define GPIO_SPEED_MEDIUM     1U
	#define GPIO_SPEED_HIGH       2U
	#define GPIO_SPEED_VERY_HIGH  3U
#endif

typedef struct{
	GPIO_TypeDef *port;
	uint8_t number;
	#if defined(STM32F103C6Tx)
	uint8_t mode;
	uint8_t cnf;
	uint8_t pull;
	#elif defined(STM32G0B1xx)
	uint8_t moder;
	uint8_t otype;
	uint8_t pull;
	uint8_t speed;
	uint8_t af;
	#endif
} GPIO_Pin_t;


uint8_t GPIO_GetPortIndex(GPIO_TypeDef *port);
void GPIO_PinMode(GPIO_Pin_t pin);
void GPIO_PinModeMulti(const GPIO_Pin_t* pins, size_t pins_number);
void GPIO_PinToggle(GPIO_Pin_t pin);
void GPIO_DigitalWrite(GPIO_Pin_t pin, uint8_t value);
uint8_t GPIO_DigitalRead(GPIO_Pin_t pin);


#endif
