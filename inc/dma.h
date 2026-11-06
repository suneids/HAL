#ifndef DMA_H
#define DMA_H
#include "gpio.h"

void DMA_Init(DMA_Channel_TypeDef *DMAx, uint32_t peripheral_addr, uint32_t memory_addr, uint16_t data_count);
void SPI1_DMA_TX_Init();
void SPI1_DMA_TX_Start(const uint8_t *buf, uint16_t len);
void SPI1_DMA_TX_Stop();
uint8_t DMA_IsReady();
#endif
