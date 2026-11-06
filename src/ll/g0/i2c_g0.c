#include "../../../inc/i2c.h"
#include <stdbool.h>
#if defined(STM32G0B1CBTx)

#define I2C_TIMEOUT 10000u
#define I2C_TIMING_100KHZ 0x00303D5Bu

#define I2C_ERROR_FLAGS (I2C_ISR_NACKF | I2C_ISR_BERR | I2C_ISR_ARLO | I2C_ISR_OVR | I2C_ISR_TIMEOUT)

static bool I2C_WaitWhile(I2C_TypeDef *I2Cx, uint32_t flag){
	uint32_t timeout = I2C_TIMEOUT;
	while((I2Cx->ISR & flag) && timeout){
		timeout--;
	}
	return timeout != 0;
}


static bool I2C_WaitUntilAny(I2C_TypeDef *I2Cx, uint32_t flags){
	uint32_t timeout = I2C_TIMEOUT;
	while(!(I2Cx->ISR & (flags | I2C_ERROR_FLAGS)) && timeout){
		timeout--;
	}
	return timeout != 0;
}


static void I2C_ClearFlags(I2C_TypeDef *I2Cx){
	I2Cx->ICR =
			I2C_ICR_STOPCF   |
			I2C_ICR_NACKCF   |
			I2C_ICR_BERRCF   |
			I2C_ICR_ARLOCF 	 |
			I2C_ICR_OVRCF 	 |
			I2C_ICR_TIMOUTCF |
			I2C_ICR_ALERTCF;
}


static bool I2C_CheckError(I2C_TypeDef *I2Cx){
	uint32_t isr = I2Cx->ISR;
	if(isr & (I2C_ISR_NACKF | I2C_ISR_BERR | I2C_ISR_ARLO | I2C_ISR_OVR | I2C_ISR_TIMEOUT)){
		I2C_ClearFlags(I2Cx);
		return false;
	}
	return true;
}


static void I2C_DelaySmall(void){
	for(volatile int d = 0; d < 500; d++) __NOP();
}


static void I2C_BusRecover(GPIO_Pin_t SDA, GPIO_Pin_t SCL){
	SDA.moder = GPIO_MODE_OUTPUT;
	SDA.otype = GPIO_OTYPE_OD;
	SDA.pull  = GPIO_PULLUP;
	SDA.speed = GPIO_SPEED_HIGH;
	SDA.af    = 0;

	SCL.moder = GPIO_MODE_OUTPUT;
	SCL.otype = GPIO_OTYPE_OD;
	SCL.pull  = GPIO_PULLUP;
	SCL.speed = GPIO_SPEED_HIGH;
	SCL.af    = 0;

	GPIO_PinMode(SDA);
	GPIO_PinMode(SCL);

	GPIO_DigitalWrite(SDA, 1);
	GPIO_DigitalWrite(SCL, 1);
	I2C_DelaySmall();

	for(int i = 0; i < 9; i++){
		GPIO_DigitalWrite(SCL, 0);
		I2C_DelaySmall();

		GPIO_DigitalWrite(SCL, 1);
		I2C_DelaySmall();
	}

	GPIO_DigitalWrite(SDA, 0);
	I2C_DelaySmall();
	GPIO_DigitalWrite(SCL, 1);
	I2C_DelaySmall();
	GPIO_DigitalWrite(SDA, 1);
	I2C_DelaySmall();

}


static void I2C_GPIO_AF_Init(GPIO_Pin_t SDA, GPIO_Pin_t SCL){
	SDA.moder = GPIO_MODE_AF;
	SDA.otype = GPIO_OTYPE_OD;
	SDA.pull  = GPIO_PULLUP;
	SDA.speed = GPIO_SPEED_HIGH;
	SDA.af    = 6;

	SCL.moder = GPIO_MODE_AF;
	SCL.otype = GPIO_OTYPE_OD;
	SCL.pull  = GPIO_PULLUP;
	SCL.speed = GPIO_SPEED_HIGH;
	SCL.af    = 6;

	GPIO_PinMode(SDA);
	GPIO_PinMode(SCL);
}


static void I2C_EnableClockAndReset(I2C_TypeDef *I2Cx){
	if(I2Cx == I2C1){
		RCC->APBENR1 |= RCC_APBENR1_I2C1EN;

		RCC->APBRSTR1 |= RCC_APBRSTR1_I2C1RST;
		RCC->APBRSTR1 &= ~RCC_APBRSTR1_I2C1RST;
	}

#ifdef I2C2
	else if(I2Cx == I2C2){

	}
#endif
}


void I2C_Init(I2C_TypeDef *I2Cx, GPIO_Pin_t SDA, GPIO_Pin_t SCL){

	I2C_BusRecover(SDA, SCL);
	I2C_GPIO_AF_Init(SDA, SCL);
	I2C_EnableClockAndReset(I2Cx);

	I2Cx->CR1 &= ~I2C_CR1_PE;
	I2Cx->TIMINGR = I2C_TIMING_100KHZ;

	I2Cx->CR1 = 0;
	I2C_ClearFlags(I2Cx);

	I2Cx->CR1 |= I2C_CR1_PE;
}


void I2C_Start(I2C_TypeDef *I2Cx){
	I2Cx->CR2 |= I2C_CR2_START;
}


static bool I2C_WaitStop(I2C_TypeDef *I2Cx){
	if(!I2C_WaitUntilAny(I2Cx, I2C_ISR_STOPF | I2C_ISR_NACKF)) return false;
	if(!I2C_CheckError(I2Cx)) return false;

	I2Cx->ICR = I2C_ICR_STOPCF;
	return true;
}


static bool I2C_WriteRaw(I2C_TypeDef *I2Cx, uint8_t devAddr,
						 const uint8_t *pData, uint16_t len){
	if(pData == 0 || len == 0 || len > 255) return false;
	if(!I2C_WaitWhile(I2Cx, I2C_ISR_BUSY))  return false;

	I2C_ClearFlags(I2Cx);

	I2Cx->CR2 = ((uint32_t)(devAddr << 1) & I2C_CR2_SADD) |
			   ((uint32_t)len << I2C_CR2_NBYTES_Pos)     |
			   I2C_CR2_AUTOEND 							 |
			   I2C_CR2_START;

	for(uint16_t i = 0; i < len; i++){
		if(!I2C_WaitUntilAny(I2Cx, I2C_ISR_TXIS | I2C_ISR_NACKF | I2C_ISR_STOPF)) return false;
		if(!I2C_CheckError(I2Cx)) 	  return false;
		if(!(I2Cx->ISR & I2C_ISR_TXIS)) return false;
		I2Cx->TXDR = pData[i];
	}

	return I2C_WaitStop(I2Cx);
}


static bool I2C_ReadRaw(I2C_TypeDef *I2Cx, uint8_t devAddr,
		 	 	 	 	uint8_t *pBuffer, uint16_t len){
	if(pBuffer == 0 || len == 0 || len > 255) return false;
	if(!I2C_WaitWhile(I2Cx, I2C_ISR_BUSY)) 	    return false;

	I2C_ClearFlags(I2Cx);
	I2Cx->CR2 = ((uint32_t)(devAddr << 1) & I2C_CR2_SADD) |
			   ((uint32_t)len << I2C_CR2_NBYTES_Pos)     |
			   I2C_CR2_RD_WRN 							 |
			   I2C_CR2_AUTOEND							 |
			   I2C_CR2_START;

	for(uint16_t i = 0; i < len; i++){
		if(!I2C_WaitUntilAny(I2Cx, I2C_ISR_RXNE | I2C_ISR_NACKF | I2C_ISR_STOPF)) return false;
		if(!I2C_CheckError(I2Cx)) 	  return false;
		if(!(I2Cx->ISR & I2C_ISR_RXNE)) return false;
		pBuffer[i] = (uint8_t)I2Cx->RXDR;
	}

	return I2C_WaitStop(I2Cx);
}


static bool I2C_WriteRegNoStop(I2C_TypeDef *I2Cx, uint8_t devAddr, uint8_t regAddr){
	if(!I2C_WaitWhile(I2Cx, I2C_ISR_BUSY)) return false;
	I2C_ClearFlags(I2Cx);
	 /*
	 * Пишем 1 байт адреса регистра.
	 * AUTOEND НЕ ставим.
	 * После передачи железо выставит TC,
	 * и мы сможем сделать repeated START на чтение.
	 */
	I2Cx->CR2 = ((uint32_t)(devAddr << 1) & I2C_CR2_SADD) |
			    (1u << I2C_CR2_NBYTES_Pos)     	          |
			    I2C_CR2_START;

	if(!I2C_WaitUntilAny(I2Cx, I2C_ISR_TXIS | I2C_ISR_NACKF)) return false;
	if(!I2C_CheckError(I2Cx)) return false;

	I2Cx->TXDR = regAddr;

	if(!I2C_WaitUntilAny(I2Cx, I2C_ISR_TC | I2C_ISR_NACKF)) return false;
	if(!I2C_CheckError(I2Cx)) return false;
	return true;
}


void I2C_WriteReg(I2C_TypeDef *I2Cx, uint8_t devAddr, uint8_t regAddr, uint8_t data){
	uint8_t tx[2];

	tx[0] = regAddr;
	tx[1] = data;
	I2C_WriteRaw(I2Cx, devAddr, tx, 2);

}


uint8_t I2C_WriteByteArray(I2C_TypeDef *I2Cx, uint8_t devAddr, uint8_t regAddr, uint8_t *pData, uint16_t len){
	if(len > 254 ) return false;
	if(len > 0 && pData == 0) return false;
	uint8_t tx[255];
	tx[0] = regAddr;

	for(uint16_t i = 0; i < len; i++) tx[i+1] = pData[i];

	return I2C_WriteRaw(I2Cx, devAddr, tx, len + 1);
}



uint8_t I2C_ReadReg(I2C_TypeDef *I2Cx, uint8_t devAddr, uint8_t regAddr){
	uint8_t data = 0;
	I2C_Read_Burst(I2Cx, devAddr, regAddr, &data, 1);
	return data;
}


uint8_t I2C_Read_Burst(I2C_TypeDef *I2Cx, uint8_t devAddr, uint8_t regAddr, uint8_t *pBuffer, uint16_t size){
	if(pBuffer == 0 || size == 0 || size > 255) return false;

	if(regAddr == I2C_NO_REG){
		I2C_ReadRaw(I2Cx, devAddr, pBuffer, size);
		return false;
	}

	if(!I2C_WriteRegNoStop(I2Cx, devAddr, regAddr)) return false;

	I2Cx->CR2 = ((uint32_t)(devAddr << 1) & I2C_CR2_SADD) |
			    ((uint32_t)size << I2C_CR2_NBYTES_Pos)    |
			    I2C_CR2_RD_WRN							  |
			    I2C_CR2_AUTOEND							  |
			    I2C_CR2_START;

	for(uint16_t i = 0; i < size; i++){
		if(!I2C_WaitUntilAny(I2Cx, I2C_ISR_RXNE | I2C_ISR_NACKF | I2C_ISR_STOPF)) return false;
		if(!I2C_CheckError(I2Cx)) return false;
		if(!(I2Cx->ISR & I2C_ISR_RXNE)) return false;

		pBuffer[i] = (uint8_t)I2Cx->RXDR;
	}
	return I2C_WaitStop(I2Cx);
}
#endif
