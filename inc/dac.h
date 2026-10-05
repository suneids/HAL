#include "mcu_config.h"
#if defined(STM32G0B1xx)
#ifndef DAC_H_
#define DAC_H_
#define DAC_MAX_VALUE 4095u

typedef enum {
    DAC_CHANNEL_1 = 1, // PA4
    DAC_CHANNEL_2 = 2  // PA5
} DAC_Channel_t;

void DAC_Init(void);

void DAC_Write(DAC_Channel_t channel, uint16_t value);

void DAC_WriteDual(uint16_t channel1, uint16_t channel2);

#endif
#endif
