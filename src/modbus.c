#include "../inc/modbus.h"
#include "../inc/tim.h"
#define COMMIT_DELAY_MS 1500
#define COMMIT_MAX_HOLD_MS 30000
#define ARRAY_LEN(a) (sizeof(a)/sizeof((a)[0]))

static void MODBUS_RxCallback(UART_HandleTypeDef* huart, uint8_t byte);
static void MODBUS_TxCallback(UART_HandleTypeDef* huart);
static void MODBUS_Timer_IRQHandler(void);
static void MODBUS_HandleReadHoldingRegisters(void);
static void MODBUS_HandleWriteSingleRegister(void);
static void MODBUS_HandleWriteMultipleRegisters(void);
static void MODBUS_SendException(uint8_t exception_code);
static void ReconfigureUARTCFG();

static const uint32_t UART_SPEEDS[] = {9600, 19200, 38400, 57600, 115200};

static volatile  ModbusFrame_t mb_frame, mb_active;
static Modbus_cfg *config;
static volatile uint8_t mb_rx_busy = 0;

uint8_t MODBUS_IsBusy(void){
    return mb_rx_busy || mb_frame.is_ready;
}


static void NV_MarkDirty(Modbus_cfg *cfg, uint32_t now_ms){
	if(!cfg->nv_dirty){
		cfg->nv_dirty = 1;
		cfg->nv_first_dirty_ms = now_ms;
	}
	cfg->nv_deadline_ms = now_ms + COMMIT_DELAY_MS;
}

static inline uint8_t TimeReached(uint32_t now, uint32_t t)
{
    return (int32_t)(now - t) >= 0;
}

uint8_t NV_CommitTimerDue(Modbus_cfg *cfg, uint32_t now_ms){
	if(!cfg->nv_dirty) return 0;
	uint8_t due_by_silence = TimeReached(now_ms, cfg->nv_deadline_ms);
	uint8_t due_by_maxhold = TimeReached(now_ms, cfg->nv_first_dirty_ms + COMMIT_MAX_HOLD_MS);
	if(!(due_by_maxhold || due_by_silence)) return 0;
	return 1;
}

void MODBUS_Init(Modbus_cfg *cfg){
	config = cfg;
	GPIO_PinMode(config->status_led);
	ReconfigureUARTCFG();
	USART_SetHandlers(config->USARTx, MODBUS_RxCallback,	MODBUS_TxCallback, config->timer);

	TIM_RegisterHandler(config->timer, MODBUS_Timer_IRQHandler);

	if(config->timer == TIM2) NVIC_EnableIRQ(TIM2_IRQn);
	//else if(config->timer == TIM3) NVIC_EnableIRQ(TIM3_IRQn);

}


static void ReconfigureUARTCFG(void)
{
	uint8_t parity = (*config->uart_cfg_reg >> 4) & 0b11;
	if (parity == 3) parity = 0;

	uint8_t stop2_flag = (*config->uart_cfg_reg >> 6) & 0b1;

	uint8_t parity_bit   = (parity != 0) ? 1 : 0;
	uint8_t stop_bits    = stop2_flag ? 2 : 1;
	uint8_t bits_per_char = 1 + 8 + parity_bit + stop_bits;

	uint16_t idx = *config->uart_cfg_reg & 0b1111;
	if (idx >= ARRAY_LEN(UART_SPEEDS)) idx = 0;
	uint32_t baud = UART_SPEEDS[idx];

	uint32_t t35_us;
	if (baud > 19200u) {
		t35_us = 1750u;
	}
	else {
		uint64_t num = 7ull * (uint64_t)bits_per_char * 1000000ull;
		uint64_t den = 2ull * (uint64_t)baud;
		t35_us = (uint32_t)((num + den - 1ull) / den);
	}

	uint32_t timclk = RCC_GetPclk1_Hz();           // TIM3 -> APB1 timer clock
	uint16_t psc    = (uint16_t)(timclk / 1000000u - 1u); // 1 tick = 1 us

	USART_Init(config->USARTx, baud, parity, stop_bits);
	TIM_Init(config->timer, psc, (uint16_t)(t35_us - 1u), 0);
}


static void MODBUS_RxCallback(UART_HandleTypeDef* huart, uint8_t byte){
	//Данные готовы
	GPIO_DigitalWrite(config->status_led, 0);
	if(mb_frame.is_ready) return;
	mb_rx_busy = 1;
	config->timer->CNT = 0;
	config->timer->CR1 |= TIM_CR1_CEN;

	//Чтение данных
	if(mb_frame.length < MB_FRAME_SIZE){
		if(mb_frame.length == 0) mb_frame.address = byte;
		else if(mb_frame.length == 1) mb_frame.function = byte;
		else{
			mb_frame.data[mb_frame.length - 2] = byte;
		}
		mb_frame.length++;
	}
	else{
		mb_frame.length = 0;
		config->timer->CR1 &= ~TIM_CR1_CEN;
		config->timer->CNT = 0;
		mb_rx_busy = 0;
	}
}


static void MODBUS_TxCallback(UART_HandleTypeDef* huart){
	if(config->uart_cfg_update){
		ReconfigureUARTCFG();
		config->uart_cfg_update = 0;
	}// DE not used: external RS485 transceiver has auto-direction

}


static void MODBUS_Timer_IRQHandler(void){
	config->timer->CR1 &= ~TIM_CR1_CEN;
	config->timer->CNT = 0;

	if(mb_frame.length > 4){
		mb_frame.is_ready = 1;
		mb_rx_busy = 0;
	}
	else{
		mb_frame.length = 0;
		mb_rx_busy = 0;
	}
}


void MODBUS_Process(void){
	if(!mb_frame.is_ready) return;

	__disable_irq();
	mb_active = mb_frame;
	mb_frame.length = 0;
	mb_frame.is_ready = 0;
	__enable_irq();
	if(mb_active.length < 5 || mb_active.length > MB_FRAME_SIZE){
		return;
	}
	if(mb_active.address == *(config->slave_id_reg) || mb_active.address == 0){
		uint8_t buffer[MB_FRAME_SIZE];
		buffer[0] = mb_active.address;
		buffer[1] = mb_active.function;
		for(uint16_t i = 0; i < mb_active.length - 2; i++){
			buffer[i + 2] = mb_active.data[i];
		}

		uint16_t received_crc = (buffer[mb_active.length-1] << 8) | buffer[mb_active.length-2];
		uint16_t calculated_crc = MODBUS_CRC16(buffer, mb_active.length - 2);
		if(received_crc != calculated_crc){
			mb_active.length = 0;
			mb_active.is_ready = 0;
			return;
		}
		uint8_t broadcast = (mb_active.address == 0x00);

		switch(mb_active.function){
			case 0x03:{
				MODBUS_HandleReadHoldingRegisters();
				break;
			}
			case 0x06:{
				MODBUS_HandleWriteSingleRegister();
				break;
			}
			case 0x10:{
				MODBUS_HandleWriteMultipleRegisters();
				break;
			}

			default:{
				if(!broadcast) MODBUS_SendException(MB_ERR_FUNCTION);
				break;
			}
		}
	}
}

static void MODBUS_HandleReadHoldingRegisters(void){

	if(mb_active.address == 0x00){
		return;
	}
	if(mb_active.length != 8){
		MODBUS_SendException(MB_ERR_VALUE);
		return;
	}


	uint16_t start_addr = (mb_active.data[0] << 8) | mb_active.data[1];
	uint16_t reg_count = (mb_active.data[2] << 8) | mb_active.data[3];
	if((uint16_t)(3u + reg_count*2u + 2u) > MB_FRAME_SIZE){
		MODBUS_SendException(MB_ERR_VALUE);
		return;
	}

	//Валидация запроса мастера
	if(reg_count > config->reg_count || reg_count < 1){
		MODBUS_SendException(MB_ERR_VALUE);
		return;
	}
	if(start_addr >= config->reg_count || (start_addr + reg_count) > config->reg_count){
		MODBUS_SendException(MB_ERR_ADDRESS);
		return;
	}

	uint8_t res[MB_FRAME_SIZE];
	res[0] = mb_active.address;
	res[1] = mb_active.function;
	res[2] = (uint8_t)(reg_count*2);

	for(uint16_t i = 0; i < reg_count; i++){
		uint16_t reg_val = config->registers[start_addr + i];
		res[3 + i*2] = (uint8_t)(reg_val >> 8); // High Byte (Big-Endian)
		res[4 + i*2] = (uint8_t)(reg_val & 0xFF);	// Low Byte
	}

	uint16_t res_len = 3 + reg_count * 2;
	uint16_t crc = MODBUS_CRC16(res, res_len);
	res[res_len++] = (uint8_t)(crc & 0xFF); // Low Byte (Low-Endian)
	res[res_len++] = (uint8_t)(crc >> 8); // High Byte

	USART_WriteLine(config->USARTx, (char*)res, res_len);
//	digitalWrite(config->status_led, 1);
}


static void MODBUS_HandleWriteSingleRegister(void){
	uint8_t broadcast = (mb_active.address == 0x00);
	if(mb_active.length != 8){
		if(!broadcast) MODBUS_SendException(MB_ERR_VALUE);
		return;
	}


	uint16_t reg_addr = (mb_active.data[0] << 8) | mb_active.data[1];
	uint16_t reg_val = (mb_active.data[2] << 8) | mb_active.data[3];
	if(reg_addr < config->reg_count){
		if(config->RegWriteAvailable && !config->RegWriteAvailable(reg_addr)){
			if(!broadcast) MODBUS_SendException(MB_ERR_ADDRESS);
			return;
		}
		uint8_t ex = 0;
		if(config->validation_exti_func)
			ex = config->validation_exti_func(config, reg_addr, reg_val);
		if(ex){
			if(!broadcast) MODBUS_SendException(ex);
			return;
		}
		config->registers[reg_addr] = reg_val;//Успешная запись
	}
	else{
		if(!broadcast) MODBUS_SendException(MB_ERR_ADDRESS);
		return;
	}
	NV_MarkDirty(config, millis()); // TODO??? Пока что сохраняем даже бродкаст
	if(!broadcast){
		uint8_t res[8];
		res[0] = mb_active.address;
		res[1] = mb_active.function;
		res[2] = mb_active.data[0];
		res[3] = mb_active.data[1];
		res[4] = mb_active.data[2];
		res[5] = mb_active.data[3];
		uint16_t crc = MODBUS_CRC16(res, 6);
		res[6] = (uint8_t)(crc & 0xFF);
		res[7] = (uint8_t)(crc >> 8);
		USART_WriteLine(config->USARTx, (char*)res, 8);
		GPIO_DigitalWrite(config->status_led, 1);
	}
	if (config->uart_cfg_update) {
		config->USARTx->CR1 |= USART_CR1_TCIE;
	}
}



static void MODBUS_HandleWriteMultipleRegisters(void){
	uint8_t broadcast = (mb_active.address == 0x00);
	if(mb_active.length < 9){
		if(!broadcast) MODBUS_SendException(MB_ERR_VALUE);
		return;
	}
	uint8_t byte_count = mb_active.data[4];
	if((uint16_t)(9u + byte_count) > MB_FRAME_SIZE){
		if(!broadcast) MODBUS_SendException(MB_ERR_VALUE);
		return;
	}
	if(mb_active.length != 9 + byte_count){
		if(!broadcast) MODBUS_SendException(MB_ERR_VALUE);
		return;
	}


	uint16_t start_addr = (mb_active.data[0] << 8) | mb_active.data[1];
	uint16_t reg_count_req = (mb_active.data[2] << 8) | mb_active.data[3];


	//Валидация запроса
	if(reg_count_req < 1 || reg_count_req > 123 || byte_count != reg_count_req * 2){
		if(!broadcast) MODBUS_SendException(MB_ERR_VALUE);
		return;
	}
	if(start_addr >= config->reg_count || (start_addr + reg_count_req) > config->reg_count){
		if(!broadcast) MODBUS_SendException(MB_ERR_ADDRESS);
		return;
	}


	for(uint16_t i = 0; i < reg_count_req; i++){
		uint16_t current_addr = start_addr + i;
		uint16_t reg_val = (mb_active.data[5 + i * 2] << 8) | mb_active.data[6 + i * 2];
		if(config->RegWriteAvailable && config->RegWriteAvailable(current_addr)){
			if(!broadcast) MODBUS_SendException(MB_ERR_ADDRESS);
			return;

		}
		uint8_t ex = 0;
		if(config->validation_exti_func)
			ex = config->validation_exti_func(config, current_addr, reg_val);
		if(ex){
			if(!broadcast) MODBUS_SendException(ex);
			return;
		}
	}

	for(uint16_t i = 0; i < reg_count_req; i++){
			uint16_t current_addr = start_addr + i;
			uint16_t reg_val = (mb_active.data[5 + i * 2] << 8) | mb_active.data[6 + i * 2];
			config->registers[current_addr] = reg_val;
	}
	NV_MarkDirty(config, millis()); // TODO??? Пока что сохраняем даже бродкаст
	if(!broadcast){
		uint8_t res[8];
		res[0] = mb_active.address;
		res[1] = mb_active.function;
		res[2] = mb_active.data[0];
		res[3] = mb_active.data[1];
		res[4] = mb_active.data[2];
		res[5] = mb_active.data[3];

		uint16_t crc = MODBUS_CRC16(res, 6);
		res[6] = (uint8_t)(crc & 0xFF);
		res[7] = (uint8_t)(crc >> 8);
		USART_WriteLine(config->USARTx, (char*)res, 8);
		GPIO_DigitalWrite(config->status_led, 1);
	}
	if (config->uart_cfg_update) {
		config->USARTx->CR1 |= USART_CR1_TCIE;
	}
}


static void MODBUS_SendException(uint8_t exception_code){
	uint8_t res[5];
	res[0] = mb_active.address;
	res[1] = mb_active.function | 0x80; //признак ошибки - старший бит
	res[2] = exception_code;
	uint16_t crc = MODBUS_CRC16(res, 3);
	res[3] = (uint8_t)(crc & 0xFF);
	res[4] = (uint8_t)(crc >> 8);
	USART_WriteLine(config->USARTx, (char*)res, 5);
}


uint16_t MODBUS_CRC16(const uint8_t *nData, uint16_t wLength){
	uint16_t wCRCWord = 0xFFFF;
	while(wLength--){
		wCRCWord ^= *(nData++);
		for(uint8_t i = 0; i < 8; i++){
			if(wCRCWord & 0x0001){
				wCRCWord >>= 1;
				wCRCWord ^= 0xA001;
			}
			else{
				wCRCWord >>= 1;
			}
		}
	}
	return wCRCWord;
}
