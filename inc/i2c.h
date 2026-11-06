#ifndef I2C_H
#define I2C_H
#define I2C_NO_REG 0xFFu
#include "gpio.h"


void I2C_Init(I2C_TypeDef *I2Cx, GPIO_Pin_t SDA, GPIO_Pin_t SCL);
void I2C_Start(I2C_TypeDef *I2Cx);
void I2C_WriteReg(I2C_TypeDef *I2Cx, uint8_t devAddr, uint8_t regAddr, uint8_t data);
uint8_t I2C_WriteByteArray(I2C_TypeDef *I2Cx, uint8_t devAddr, uint8_t regAddr, uint8_t *pData, uint16_t len);
uint8_t I2C_ReadReg(I2C_TypeDef *I2Cx, uint8_t devAddr, uint8_t regAddr);
uint8_t I2C_Read_Burst(I2C_TypeDef *I2Cx, uint8_t devAddr, uint8_t regAddr, uint8_t *pBuffer, uint16_t size);
#endif
