#include "mcu_config.h"
#if defined(STM32G0B1xx)
#ifndef FDCAN_H
#define FDCAN_H

uint8_t FDCAN_Init(FDCAN_GlobalTypeDef *FDCANx, uint32_t bitrate);

uint8_t FDCAN_Send(FDCAN_GlobalTypeDef *FDCANx, uint32_t id, const uint8_t *data, uint8_t len);

uint8_t FDCAN_Read(FDCAN_GlobalTypeDef *FDCANx, uint32_t *id, uint8_t *data, uint8_t *len);

//void FDCAN_SetFilter(...);
//void FDCAN_RegisterHandler(...);
#endif
#endif
