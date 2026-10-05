#include "../../../inc/mcu_config.h"
#if defined(STM32G0B1xx)
#include "../../../inc/spi.h"
#include "../../../inc/tim.h"
#include "../../../inc/dma.h"


static SPI_Status_t enableSPIClock(void *instance){
	if(instance == SPI1){
		RCC->APBENR2 |= RCC_APBENR2_SPI1EN;
		(void)RCC->APBENR2;

		RCC->APBRSTR2 |= RCC_APBRSTR2_SPI1RST;
		RCC->APBRSTR2 &= ~RCC_APBRSTR2_SPI1RST;

		return SPI_OK;
	}

#ifdef SPI2
	if(instance == SPI2){
		RCC->APBENR1 |= RCC_APBENR1_SPI2EN;
		(void)RCC->APBENR1;

		RCC->APBRSTR1 |= RCC_APBRSTR1_SPI2RST;
		RCC->APBRSTR1 &= ~RCC_APBRSTR1_SPI2RST;

		return SPI_OK;
	}
#endif

#ifdef SPI3
	if(instance == SPI3){
		RCC->APBENR1 |= RCC_APBENR1_SPI3EN;
		(void)RCC->APBENR1;

		RCC->APBRSTR1 |= RCC_APBRSTR1_SPI3RST;
		RCC->APBRSTR1 &= ~RCC_APBRSTR1_SPI3RST;

		return SPI_OK;
	}
#endif

	return SPI_ERR_PARAM;
}


static inline uint32_t SPI_BR_Bits(SPI_BaudDiv_t div)
{
    switch(div)
    {
        case SPI_BAUD_DIV_2:   return 0u << SPI_CR1_BR_Pos;
        case SPI_BAUD_DIV_4:   return 1u << SPI_CR1_BR_Pos;
        case SPI_BAUD_DIV_8:   return 2u << SPI_CR1_BR_Pos;
        case SPI_BAUD_DIV_16:  return 3u << SPI_CR1_BR_Pos;
        case SPI_BAUD_DIV_32:  return 4u << SPI_CR1_BR_Pos;
        case SPI_BAUD_DIV_64:  return 5u << SPI_CR1_BR_Pos;
        case SPI_BAUD_DIV_128: return 6u << SPI_CR1_BR_Pos;
        case SPI_BAUD_DIV_256: return 7u << SPI_CR1_BR_Pos;
        default:  			   return 0xFFFFFFFFu;
    }
}

static void SPI_FlushRx(SPI_TypeDef *SPIx){
	while((SPIx->SR & SPI_SR_FRLVL) != 0u){
		(void)*(__IO uint8_t *)&SPIx->DR;
	}

	/*
	 * OVR очищается последовательностью:
	 * чтение DR, затем чтение SR.
	 */
	if((SPIx->SR & SPI_SR_OVR) != 0u){
		(void)*(__IO uint8_t *)&SPIx->DR;
		(void)SPIx->SR;
	}
}

SPI_Status_t SPI_Init(SPI_Handle_t *h, SPI_Config_t *cfg){
	if(!h || !cfg || !cfg->instance){
		return SPI_ERR_PARAM;
	}

	SPI_Status_t st = enableSPIClock(cfg->instance);
	if(st != SPI_OK){
		return st;
	}

	SPI_TypeDef *SPIx = (SPI_TypeDef *)cfg->instance;
	cfg->sck.moder = GPIO_MODE_AF;
	cfg->sck.otype = GPIO_OTYPE_PP;
	cfg->sck.pull  = GPIO_NOPULL;
	cfg->sck.speed = GPIO_SPEED_VERY_HIGH;

	cfg->mosi.moder = GPIO_MODE_AF;
	cfg->mosi.otype = GPIO_OTYPE_PP;
	cfg->mosi.pull  = GPIO_NOPULL;
	cfg->mosi.speed = GPIO_SPEED_VERY_HIGH;

	cfg->miso.moder = GPIO_MODE_AF;
	cfg->miso.otype = GPIO_OTYPE_PP;
	cfg->miso.pull  = GPIO_NOPULL;
	cfg->miso.speed = GPIO_SPEED_VERY_HIGH;

	GPIO_PinMode(cfg->sck);
	GPIO_PinMode(cfg->mosi);
	GPIO_PinMode(cfg->miso);

	uint32_t br = SPI_BR_Bits(cfg->baud_div);
	if(br == 0xFFFFFFFFu)
	{
		return SPI_ERR_PARAM;
	}

	h->instance     = cfg->instance;
	h->sck          = cfg->sck;
	h->mosi         = cfg->mosi;
	h->miso         = cfg->miso;
	h->mode         = cfg->mode;
	h->bitorder     = cfg->bitorder;
	h->baud_div     = cfg->baud_div;
	h->software_nss = cfg->software_nss;
	h->busy         = false;

	/* Перед конфигурированием отключаем SPI */
	SPIx->CR1 &= ~SPI_CR1_SPE;

#ifdef SPI_I2SCFGR_I2SMOD
	/* Явно включаем режим SPI, а не I2S */
	SPIx->I2SCFGR &= ~SPI_I2SCFGR_I2SMOD;
#endif

	uint32_t cr1 = SPI_CR1_MSTR | br;

	switch(cfg->mode){
		case SPI_MODE0:
			break;

		case SPI_MODE1:
			cr1 |= SPI_CR1_CPHA;
			break;

		case SPI_MODE2:
			cr1 |= SPI_CR1_CPOL;
			break;

		case SPI_MODE3:
			cr1 |= SPI_CR1_CPOL | SPI_CR1_CPHA;
			break;

		default:
			return SPI_ERR_PARAM;
	}

	if(cfg->bitorder == SPI_LSB_FIRST){
		cr1 |= SPI_CR1_LSBFIRST;
	}

	if(cfg->software_nss){
		cr1 |= SPI_CR1_SSM | SPI_CR1_SSI;
	}
	else{
		/*
		 * Твой драйвер управляет CS отдельным GPIO,
		 * поэтому аппаратный NSS пока не поддерживается.
		 */
		return SPI_ERR_PARAM;
	}

	/*
	 * DS = 7:
	 * размер кадра = DS + 1 = 8 бит.
	 *
	 * FRXTH = 1:
	 * RXNE срабатывает при наличии минимум 8 бит,
	 * поэтому DR можно читать через uint8_t.
	 */
	uint32_t cr2 =
		  (7u << SPI_CR2_DS_Pos)
		| SPI_CR2_FRXTH;

	SPIx->CR1 = cr1;
	SPIx->CR2 = cr2;

	SPI_FlushRx(SPIx);

	SPIx->CR1 |= SPI_CR1_SPE;

	return SPI_OK;
}



SPI_Status_t SPI_SetBaud(SPI_Handle_t *h, SPI_BaudDiv_t baud_div)
{
    if(!h || !h->instance)
    {
        return SPI_ERR_PARAM;
    }

    if(h->busy)
    {
        return SPI_ERR_BUSY;
    }

    uint32_t br = SPI_BR_Bits(baud_div);
    if(br == 0xFFFFFFFFu)
    {
        return SPI_ERR_PARAM;
    }

    SPI_TypeDef *SPIx = (SPI_TypeDef *)h->instance;

    uint32_t cr1 = SPIx->CR1;

    SPIx->CR1 = cr1 & ~SPI_CR1_SPE;

    cr1 &= ~SPI_CR1_BR;
    cr1 |= br;

    SPIx->CR1 = cr1;
    SPIx->CR1 = cr1 | SPI_CR1_SPE;

    h->baud_div = baud_div;

    return SPI_OK;
}


SPI_Status_t SPI_SetMode(SPI_Handle_t *h, SPI_Mode_t mode){
	if(!h || !h->instance) return SPI_ERR_PARAM;
	if(h->busy) return SPI_ERR_BUSY;
	if(mode > SPI_MODE3) return SPI_ERR_PARAM;

	SPI_TypeDef *SPIx = (SPI_TypeDef*)h->instance;
	uint32_t cr1 = SPIx->CR1;
	SPIx->CR1 = cr1 & ~SPI_CR1_SPE;

	cr1 &= ~(SPI_CR1_CPOL | SPI_CR1_CPHA);

	switch(mode){
		case SPI_MODE0: break;
		case SPI_MODE1: cr1 |= SPI_CR1_CPHA; break;
		case SPI_MODE2: cr1 |= SPI_CR1_CPOL; break;
		case SPI_MODE3: cr1 |= (SPI_CR1_CPHA | SPI_CR1_CPOL); break;
	}

	SPIx->CR1 = cr1;
	SPIx->CR1 = cr1 | SPI_CR1_SPE;
	h->mode = mode;
	return SPI_OK;
}


static inline void SPI_CS(SPI_Device_t *dev, bool active){
	uint8_t level;
	if(dev->cs_active_low) level = active? 0u : 1u;
	else				   level = active? 1u : 0u;
	GPIO_DigitalWrite(dev->cs, level);
}


SPI_Status_t SPI_DeviceInit(
	SPI_Device_t *dev,
	SPI_Handle_t *bus,
	GPIO_Pin_t cs,
	bool cs_active_low)
{
	if(!dev || !bus || !bus->instance){
		return SPI_ERR_PARAM;
	}

	cs.moder = GPIO_MODE_OUTPUT;
	cs.otype = GPIO_OTYPE_PP;
	cs.pull  = GPIO_NOPULL;
	cs.speed = GPIO_SPEED_HIGH;
	cs.af    = 0u; /* В режиме OUTPUT не используется */

	dev->bus = bus;
	dev->cs = cs;
	dev->cs_active_low = cs_active_low;

	GPIO_PinMode(cs);

	SPI_CS(dev, false);

	return SPI_OK;
}


static bool SPI_WaitSet(volatile uint32_t *reg, uint32_t mask, uint32_t timeout_ms){
	uint32_t t0 = millis();
	while(((*reg) & mask) == 0u){
		if((uint32_t)(millis() - t0) >= timeout_ms) return false;
	}
	return true;
}


static bool SPI_WaitClr(volatile uint32_t *reg, uint32_t mask, uint32_t timeout_ms){
	uint32_t t0 = millis();
	while(((*reg) & mask) != 0u){
		if((uint32_t)(millis() - t0) >= timeout_ms) return false;
	}
	return true;
}


SPI_Status_t SPI_Transfer(
    SPI_Device_t *dev,
    const uint8_t *tx,
    uint8_t *rx,
    size_t n,
    uint8_t dummy)
{
    if(!dev || !dev->bus || !dev->bus->instance || n == 0u){
        return SPI_ERR_PARAM;
    }

    SPI_Handle_t *h = dev->bus;

    if(h->busy){
        return SPI_ERR_BUSY;
    }

    h->busy = true;

    SPI_TypeDef *SPIx = (SPI_TypeDef *)h->instance;
    SPI_Status_t st = SPI_OK;

    SPI_FlushRx(SPIx);
    SPI_CS(dev, true);

    for(size_t i = 0; i < n; i++){
        uint8_t out = tx ? tx[i] : dummy;

        if(!SPI_WaitSet(&SPIx->SR, SPI_SR_TXE, 5u)){
            st = SPI_ERR_HW;
            break;
        }

        /*
         * Обязательно 8-битная запись.
         * Иначе компилятор может выполнить 16-битную запись в DR.
         */
        *(__IO uint8_t *)&SPIx->DR = out;

        if(!SPI_WaitSet(&SPIx->SR, SPI_SR_RXNE, 5u)){
            st = SPI_ERR_HW;
            break;
        }

        uint8_t in = *(__IO uint8_t *)&SPIx->DR;

        if(rx){
            rx[i] = in;
        }

        if((SPIx->SR & (SPI_SR_OVR | SPI_SR_MODF | SPI_SR_FRE)) != 0u){
            st = SPI_ERR_HW;
            break;
        }
    }

    if(st == SPI_OK){
        /* Перед снятием CS TX FIFO должен опустеть */
        if(!SPI_WaitClr(&SPIx->SR, SPI_SR_FTLVL, 5u)){
            st = SPI_ERR_HW;
        }

        /* И последний бит физически должен уйти с линии */
        if((st == SPI_OK) && !SPI_WaitClr(&SPIx->SR, SPI_SR_BSY, 5u)){
            st = SPI_ERR_HW;
        }
    }
    else{
        (void)SPI_WaitClr(&SPIx->SR, SPI_SR_BSY, 2u);
        SPI_FlushRx(SPIx);
    }

    SPI_CS(dev, false);
    h->busy = false;

    return st;
}


SPI_Status_t SPI_Write(SPI_Device_t *dev, const uint8_t *tx, size_t n){
	return SPI_Transfer(dev, tx, NULL, n, 0x00u);
}


SPI_Status_t SPI_Read(SPI_Device_t *dev, uint8_t *rx, size_t n, uint8_t dummy){
	if(!rx) return SPI_ERR_PARAM;
	return SPI_Transfer(dev, NULL, rx, n, dummy);
}


static SPI_Status_t SPI_RegReadBytes(SPI_Device_t *dev, uint8_t addr, uint8_t read_mask, uint8_t *out, size_t n, uint8_t dummy){
	if(!dev || !out || n == 0u) return SPI_ERR_PARAM;

	if(n > 8u) return SPI_ERR_PARAM;

	uint8_t tx[1+8];
	uint8_t rx[1+8];

	tx[0] = (uint8_t)(addr | read_mask);
	for(size_t i = 0; i < n; i++) tx[1 + i] = dummy;

	SPI_Status_t st = SPI_Transfer(dev, tx, rx, 1u + n, dummy);
	if(st != SPI_OK) return st;

	for(size_t i = 0; i < n; i++) out[i] = rx[1+i];
	return SPI_OK;
}


SPI_Status_t SPI_ReadReg8(SPI_Device_t *dev, uint8_t addr, uint8_t read_mask, uint8_t *val){
	return SPI_RegReadBytes(dev, addr, read_mask, val, 1u, 0x00u);
}


SPI_Status_t SPI_ReadReg16BE(SPI_Device_t *dev, uint8_t addr, uint8_t read_mask, uint16_t *val){
	if(!val) return SPI_ERR_PARAM;

	uint8_t b[2];
	SPI_Status_t st = SPI_RegReadBytes(dev, addr, read_mask, b, 2u, 0x00u);
	if(st != SPI_OK) return st;

	*val = (uint16_t)((uint16_t)b[0] << 8) | (uint16_t)b[1];
	return SPI_OK;
}


SPI_Status_t SPI_ReadReg16LE(SPI_Device_t *dev, uint8_t addr, uint8_t read_mask, uint16_t *val){
	if(!val) return SPI_ERR_PARAM;

	uint8_t b[2];
	SPI_Status_t st = SPI_RegReadBytes(dev, addr, read_mask, b, 2u, 0x00u);
	if(st != SPI_OK) return st;

	*val = (uint16_t)((uint16_t)b[1] << 8) | (uint16_t)b[0];
	return SPI_OK;
}


SPI_Status_t SPI_ReadReg24BE(SPI_Device_t *dev, uint8_t addr, uint8_t read_mask, uint32_t *val){
	if(!val) return SPI_ERR_PARAM;

	uint8_t b[3];
	SPI_Status_t st = SPI_RegReadBytes(dev, addr, read_mask, b, 3u, 0x00u);
	if(st != SPI_OK) return st;

	*val = ((uint32_t)b[0] << 16) | ((uint32_t)b[1] << 8) | (uint32_t)b[2];
	return SPI_OK;
}


SPI_Status_t SPI_ReadReg24LE(SPI_Device_t *dev, uint8_t addr, uint8_t read_mask, uint32_t *val){
	if(!val) return SPI_ERR_PARAM;

	uint8_t b[3];
	SPI_Status_t st = SPI_RegReadBytes(dev, addr, read_mask, b, 3u, 0x00u);
	if(st != SPI_OK) return st;

	*val = ((uint32_t)b[2] << 16) | ((uint32_t)b[1] << 8) | (uint32_t)b[0];
	return SPI_OK;
}


SPI_Status_t SPI_WriteReg8(SPI_Device_t *dev, uint8_t addr, uint8_t write_mask, uint8_t val){
	if(!dev) return SPI_ERR_PARAM;

	uint8_t tx[2];
	tx[0] = (uint8_t)(addr & (uint8_t)~write_mask);
	tx[1] = val;

	return SPI_Write(dev, tx, 2u);
}


SPI_Status_t SPI_WriteReg16BE(SPI_Device_t *dev, uint8_t addr, uint8_t write_mask, uint16_t val){
	if(!dev) return SPI_ERR_PARAM;

	uint8_t tx[3];
	tx[0] = (uint8_t)(addr & (uint8_t)~write_mask);
	tx[1] = (uint8_t)(val >> 8);
	tx[2] = (uint8_t)(val & 0xFF);

	return SPI_Write(dev, tx, 3u);
}


SPI_Status_t SPI_WriteReg16LE(SPI_Device_t *dev, uint8_t addr, uint8_t write_mask, uint16_t val){
	if(!dev) return SPI_ERR_PARAM;

	uint8_t tx[3];
	tx[0] = (uint8_t)(addr & (uint8_t)~write_mask);
	tx[1] = (uint8_t)(val & 0xFF);
	tx[2] = (uint8_t)(val >> 8);

	return SPI_Write(dev, tx, 3u);
}


void SPI1_DMA_Send(uint8_t *buf, uint16_t len){
	SPI1->CR2 |= SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN;
}


#endif
