#ifndef SPI_H
#define SPI_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "gpio.h"


typedef enum{
	SPI_MODE0 = 0,
	SPI_MODE1,
	SPI_MODE2,
	SPI_MODE3
}SPI_Mode_t;

typedef enum {
	SPI_MSB_FIRST = 0,
	SPI_LSB_FIRST = 1
}SPI_Bitorder_t;

typedef enum{
	SPI_OK 		  = 0,
	SPI_ERR_PARAM = -1,
	SPI_ERR_BUSY  = -2,
	SPI_ERR_HW 	  = -3
}SPI_Status_t;

typedef enum {
    SPI_BAUD_DIV_2   = 2,
    SPI_BAUD_DIV_4   = 4,
    SPI_BAUD_DIV_8   = 8,
    SPI_BAUD_DIV_16  = 16,
    SPI_BAUD_DIV_32  = 32,
    SPI_BAUD_DIV_64  = 64,
    SPI_BAUD_DIV_128 = 128,
    SPI_BAUD_DIV_256 = 256
} SPI_BaudDiv_t;


typedef struct{
	void *instance;
	GPIO_Pin_t sck, miso, mosi;

	SPI_Mode_t mode;
	SPI_Bitorder_t bitorder;

	SPI_BaudDiv_t baud_div;
	bool software_nss;
}SPI_Config_t;


typedef struct{
	void *instance;
	GPIO_Pin_t sck, miso, mosi;

	SPI_Mode_t mode;
	SPI_Bitorder_t bitorder;
	SPI_BaudDiv_t baud_div;
	bool software_nss;

	volatile bool busy;
}SPI_Handle_t;


typedef struct{
	SPI_Handle_t *bus;
	GPIO_Pin_t cs;
	bool cs_active_low;
}SPI_Device_t;


SPI_Status_t SPI_Init(SPI_Handle_t *h, SPI_Config_t *cfg);

SPI_Status_t SPI_SetBaud(SPI_Handle_t *h, SPI_BaudDiv_t baud_div);
SPI_Status_t SPI_SetMode(SPI_Handle_t *h, SPI_Mode_t mode);

SPI_Status_t SPI_DeviceInit(SPI_Device_t *dev, SPI_Handle_t *bus, GPIO_Pin_t cs, bool cs_active_low);

SPI_Status_t SPI_Transfer(SPI_Device_t *dev, const uint8_t *tx, uint8_t *rx, size_t n, uint8_t dummy);

SPI_Status_t SPI_Write(SPI_Device_t *dev, const uint8_t *tx, size_t n);
SPI_Status_t SPI_Read(SPI_Device_t *dev, uint8_t *rx, size_t n, uint8_t dummy);

SPI_Status_t SPI_ReadReg8(SPI_Device_t *dev, uint8_t addr, uint8_t read_mask, uint8_t *val);
SPI_Status_t SPI_ReadReg16BE(SPI_Device_t *dev, uint8_t addr, uint8_t read_mask, uint16_t *val);
SPI_Status_t SPI_ReadReg16LE(SPI_Device_t *dev, uint8_t addr, uint8_t read_mask, uint16_t *val);
SPI_Status_t SPI_ReadReg24BE(SPI_Device_t *dev, uint8_t addr, uint8_t read_mask, uint32_t *val);
SPI_Status_t SPI_ReadReg24LE(SPI_Device_t *dev, uint8_t addr, uint8_t read_mask, uint32_t *val);

SPI_Status_t SPI_WriteReg8(SPI_Device_t *dev, uint8_t addr, uint8_t write_mask, uint8_t val);
SPI_Status_t SPI_WriteReg16BE(SPI_Device_t *dev, uint8_t addr, uint8_t write_mask, uint16_t val);
SPI_Status_t SPI_WriteReg16LE(SPI_Device_t *dev, uint8_t addr, uint8_t write_mask, uint16_t val);

//SPI_Status_t SPI_TransferAsync();

void SPI1_DMA_Send(uint8_t *buf, uint16_t len);
#endif
