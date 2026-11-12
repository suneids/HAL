#ifndef DMA_H
#define DMA_H
#include "gpio.h"

typedef enum {
    DMA_EVENT_HT,
    DMA_EVENT_TC
} DMA_Event_t;

void DMA_Init(DMA_Channel_TypeDef *DMAx, uint32_t peripheral_addr, uint32_t memory_addr, uint16_t data_count);
void SPI1_DMA_TX_Init();
void SPI1_DMA_TX_Start(const uint8_t *buf, uint16_t len);
void SPI1_DMA_TX_Stop();
uint8_t DMA_IsReady(DMA_Channel_TypeDef *DMAx);


typedef void (*DMA_Callback_t)(DMA_Event_t event);
#if defined(STM32G0B1xx)
void DMA_SetHandler(DMA_Channel_TypeDef *DMAx, DMA_Callback_t callback);
#endif
#endif
