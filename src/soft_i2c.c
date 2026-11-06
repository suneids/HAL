#include "../inc/soft_i2c.h"
static void I2C_SET(GPIO_Pin_t pin){
	pin.port->BSRR = (1 <<pin.number);
}


static void I2C_RESET(GPIO_Pin_t pin){
	pin.port->BRR = (1 <<pin.number);
}


static uint8_t I2C_READ(GPIO_Pin_t pin){
	return (pin.port->IDR & (1 << pin.number)) ? 1 : 0;
}

void SoftI2C_Start(GPIO_Pin_t sda, GPIO_Pin_t scl){
	I2C_SET(sda);
	I2C_SET(scl);
	I2C_DELAY();
	I2C_RESET(sda);
	I2C_DELAY();
	I2C_RESET(scl);
	I2C_DELAY();
}

void SoftI2C_Stop(GPIO_Pin_t sda, GPIO_Pin_t scl){
	I2C_RESET(sda);
	I2C_DELAY();
	I2C_SET(scl);
	I2C_DELAY();
	I2C_SET(sda);
	I2C_DELAY();

}


uint8_t SoftI2C_Write(GPIO_Pin_t sda, GPIO_Pin_t scl, uint8_t byte){
	for(uint8_t i = 0; i<8; i++){
		if(byte & 0x80)	I2C_SET(sda);
		else I2C_RESET(sda);
		I2C_DELAY();
		I2C_SET(scl);
		I2C_DELAY();
		I2C_RESET(scl);
		byte <<= 1;
	}

	I2C_SET(sda);
	I2C_DELAY();
	I2C_SET(scl);
	I2C_DELAY();
	uint8_t ack = I2C_READ(sda);
	I2C_RESET(scl);
	I2C_DELAY();
	return ack;
}


uint8_t SoftI2C_Read(GPIO_Pin_t sda, GPIO_Pin_t scl, uint8_t ack) {
    uint8_t byte = 0;
    I2C_SET(sda); // Отпускаем SDA, чтобы датчик мог ею рулить
    I2C_DELAY();

    for(uint8_t i = 0; i < 8; i++) {
        I2C_SET(scl);      // Поднимаем SCL
        I2C_DELAY();       // <--- ДАЕМ ВРЕМЯ сигналу долететь до пина
        I2C_DELAY();       // Еще немного для верности

        byte <<= 1;
        if(I2C_READ(sda)) byte |= 1; // Читаем, пока SCL стабильно в 1

        I2C_RESET(scl);    // Опускаем SCL
        I2C_DELAY();       // Даем датчику время подготовить следующий бит
    }

    // Формируем ACK/NACK
    if(!ack) I2C_RESET(sda); // ACK (0)
    else I2C_SET(sda);       // NACK (1)

    I2C_DELAY();
    I2C_SET(scl);
    I2C_DELAY();
    I2C_RESET(scl);
    I2C_SET(sda); // Освобождаем шину
    return byte;
}
