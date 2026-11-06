#ifndef MODBUS_H
#define MODBUS_H

#include "usart.h"

#define MB_FRAME_SIZE   256

#define MB_ERR_NONE     0x00
#define MB_ERR_FUNCTION 0x01
#define MB_ERR_ADDRESS  0x02
#define MB_ERR_VALUE    0x03

typedef struct Modbus_cfg Modbus_cfg;
typedef struct{
	uint8_t  address;
	uint8_t  function;
	uint8_t  data[MB_FRAME_SIZE - 4];
	uint16_t crc;
	uint16_t length;
	uint8_t  is_ready;
}ModbusFrame_t;

typedef uint8_t (*Modbus_ValidateEXT)(Modbus_cfg* cfg, uint16_t reg_addr, uint16_t new_val);
typedef uint8_t (*Modbus_IsWritableRegEXT)(uint16_t reg);

struct Modbus_cfg{
	USART_TypeDef *USARTx;
	TIM_TypeDef *timer;
	uint16_t *slave_id_reg;
	GPIO_Pin_t status_led;
	uint16_t *registers;
	uint16_t reg_count;
	Modbus_IsWritableRegEXT RegWriteAvailable;
	uint16_t *uart_cfg_reg;
	Modbus_ValidateEXT validation_exti_func;
	uint8_t uart_cfg_update;
	volatile uint8_t nv_dirty;
	uint32_t nv_deadline_ms;
	uint32_t nv_first_dirty_ms;
};

uint8_t MODBUS_IsBusy(void);
void MODBUS_Init(Modbus_cfg *cfg);
void MODBUS_Process(void);
uint16_t MODBUS_CRC16(const uint8_t *nData, uint16_t wLength);
uint8_t NV_CommitTimerDue(Modbus_cfg *cfg, uint32_t now_ms);

#endif
