#include "../../../inc/dma.h"

#if defined(STM32G0B1xx)

#ifndef DMA_REQUEST_ADC1
#define DMA_REQUEST_ADC1 5u
#endif

static uint8_t DMA_GetChannelIndex(DMA_Channel_TypeDef *DMAx){
	uint8_t result = 0xFF;
	if(DMAx == DMA1_Channel1) 	   result = 0;
	else if(DMAx == DMA1_Channel2) result = 1;
	else if(DMAx == DMA1_Channel3) result = 2;
	else if(DMAx == DMA1_Channel4) result = 3;
	else if(DMAx == DMA1_Channel5) result = 4;

#ifdef DMA1_Channel6
	else if(DMAx == DMA1_Channel6) result = 5;
#endif

#ifdef DMA1_Channel7
	else if(DMAx == DMA1_Channel7) result = 6;
#endif

	return result;
}


static void DMA_SetRequest(DMA_Channel_TypeDef *DMAx, uint32_t request){
	uint8_t index = DMA_GetChannelIndex(DMAx);
	request &= DMAMUX_CxCR_DMAREQ_ID_Msk;

	switch(index){
		case 0   : DMAMUX1_Channel0->CCR = request; break;
		case 1   : DMAMUX1_Channel1->CCR = request; break;
		case 2   : DMAMUX1_Channel2->CCR = request; break;
		case 3   : DMAMUX1_Channel3->CCR = request; break;
		case 4   : DMAMUX1_Channel4->CCR = request; break;
#ifdef DMA1_Channel6
		case 5   : DMAMUX1_Channel5->CCR = request; break;
#endif

#ifdef DMA1_Channel7
		case 6   : DMAMUX1_Channel6->CCR = request; break;
#endif
		case 0xFF:
		default  : return;
	}
}


static void DMA_ClearFlags(DMA_Channel_TypeDef *DMAx){
	uint8_t index = DMA_GetChannelIndex(DMAx);
	if(index == 0xFF) return;
	DMA1->IFCR = (0x0Fu << (index * 4u));
}


void DMA_Init(DMA_Channel_TypeDef *DMAx, uint32_t peripheral_addr, uint32_t memory_addr, uint16_t data_count){
	if(DMAx == 0 || peripheral_addr == 0 || memory_addr == 0 || data_count == 0) return;
	RCC->AHBENR |= RCC_AHBENR_DMA1EN;
#ifdef RCC_AHBENR_DMAMUX1EN
	RCC->AHBENR |= RCC_AHBENR_DMAMUX1EN;
#endif
	DMAx->CCR &= ~DMA_CCR_EN;

	DMA_ClearFlags(DMAx);

	if(peripheral_addr == (uint32_t)&ADC1->DR) DMA_SetRequest(DMAx, DMA_REQUEST_ADC1);


	DMAx->CPAR = peripheral_addr;
	DMAx->CMAR = memory_addr;
	DMAx->CNDTR = data_count;

	DMAx->CCR = DMA_CCR_MINC 	 |
			     DMA_CCR_CIRC  	 |
				 DMA_CCR_MSIZE_0 |
				 DMA_CCR_PSIZE_0 |
				 DMA_CCR_PL_0;

	DMAx->CCR |= DMA_CCR_EN;

}


void SPI1_DMA_TX_Init(void)
{

}


void SPI1_DMA_TX_Start(const uint8_t *buf, uint16_t len){

}


void SPI1_DMA_TX_Stop(){

}

uint8_t DMA_IsReady(DMA_Channel_TypeDef *DMAx)
{
    uint8_t index = DMA_GetChannelIndex(DMAx);
    if (index == 0xFFu) return 0u;

    uint32_t tc_mask = DMA_ISR_TCIF1 << (index * 4u);

    if ((DMA1->ISR & tc_mask) == 0u)
        return 0u;

    /* Сбрасываем флаг, чтобы следующий проход снова дал Ready */
    DMA1->IFCR = DMA_IFCR_CTCIF1 << (index * 4u);

    return 1u;
}
#endif
