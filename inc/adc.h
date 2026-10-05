#ifndef ADC_H
#define ADC_H
#define DMA_ENABLE 1
#define DMA_DISABLE 0
#include "gpio.h"

void ADC_InitMulti(GPIO_Pin_t *pins, uint16_t count, uint8_t need_dma);
void ADC_Start();
#endif
