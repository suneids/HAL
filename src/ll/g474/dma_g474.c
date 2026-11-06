#include "../../../inc/dma.h"

#if defined(STM32G474xx)

#ifndef DMA_REQUEST_ADC1
#define DMA_REQUEST_ADC1 5u
#endif


static DMA_TypeDef *DMA_GetController(DMA_Channel_TypeDef *DMAx){

	if(DMAx == DMA1_Channel1 ||
	   DMAx == DMA1_Channel2 ||
	   DMAx == DMA1_Channel3 ||
	   DMAx == DMA1_Channel4 ||
	   DMAx == DMA1_Channel5 ||
	   DMAx == DMA1_Channel6 ||
	   DMAx == DMA1_Channel7 ||
	   DMAx == DMA1_Channel8){

		return DMA1;
	}

	if(DMAx == DMA2_Channel1 ||
	   DMAx == DMA2_Channel2 ||
	   DMAx == DMA2_Channel3 ||
	   DMAx == DMA2_Channel4 ||
	   DMAx == DMA2_Channel5 ||
	   DMAx == DMA2_Channel6 ||
	   DMAx == DMA2_Channel7 ||
	   DMAx == DMA2_Channel8){

		return DMA2;
	}

	return NULL;
}


static uint8_t DMA_GetChannelIndex(DMA_Channel_TypeDef *DMAx){

	if(DMAx == DMA1_Channel1 || DMAx == DMA2_Channel1) return 0;
	if(DMAx == DMA1_Channel2 || DMAx == DMA2_Channel2) return 1;
	if(DMAx == DMA1_Channel3 || DMAx == DMA2_Channel3) return 2;
	if(DMAx == DMA1_Channel4 || DMAx == DMA2_Channel4) return 3;
	if(DMAx == DMA1_Channel5 || DMAx == DMA2_Channel5) return 4;
	if(DMAx == DMA1_Channel6 || DMAx == DMA2_Channel6) return 5;
	if(DMAx == DMA1_Channel7 || DMAx == DMA2_Channel7) return 6;
	if(DMAx == DMA1_Channel8 || DMAx == DMA2_Channel8) return 7;

	return 0xFF;
}


static uint8_t DMA_GetDMAMUXIndex(DMA_Channel_TypeDef *DMAx){

	uint8_t index = DMA_GetChannelIndex(DMAx);

	if(index == 0xFF) return 0xFF;

	if(DMA_GetController(DMAx) == DMA1){
		return index;
	}

	if(DMA_GetController(DMAx) == DMA2){
		return index + 8u;
	}

	return 0xFF;
}


static void DMA_SetRequest(DMA_Channel_TypeDef *DMAx, uint32_t request){

	uint8_t index = DMA_GetDMAMUXIndex(DMAx);

	if(index == 0xFF) return;

	request &= DMAMUX_CxCR_DMAREQ_ID_Msk;

	volatile DMAMUX_Channel_TypeDef *mux =
		(volatile DMAMUX_Channel_TypeDef *)
		(DMAMUX1_Channel0_BASE +
		 index * sizeof(DMAMUX_Channel_TypeDef));

	mux->CCR = request;
}


static void DMA_ClearFlags(DMA_Channel_TypeDef *DMAx){

	DMA_TypeDef *DMA = DMA_GetController(DMAx);
	uint8_t index = DMA_GetChannelIndex(DMAx);

	if(DMA == NULL || index == 0xFF) return;

	DMA->IFCR = (0x0Fu << (index * 4u));
}


void DMA_Init(DMA_Channel_TypeDef *DMAx,
			  uint32_t peripheral_addr,
			  uint32_t memory_addr,
			  uint16_t data_count){

	if(DMAx == NULL ||
	   peripheral_addr == 0 ||
	   memory_addr == 0 ||
	   data_count == 0){

		return;
	}

	DMA_TypeDef *DMA = DMA_GetController(DMAx);

	if(DMA == NULL) return;


	// DMAMUX общий для DMA1/DMA2
	RCC->AHB1ENR |= RCC_AHB1ENR_DMAMUX1EN;


	if(DMA == DMA1){
		RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
	}
	else if(DMA == DMA2){
		RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
	}


	DMAx->CCR &= ~DMA_CCR_EN;

	DMA_ClearFlags(DMAx);


	if(peripheral_addr == (uint32_t)&ADC1->DR){
		DMA_SetRequest(DMAx, DMA_REQUEST_ADC1);
	}


	DMAx->CPAR  = peripheral_addr;
	DMAx->CMAR  = memory_addr;
	DMAx->CNDTR = data_count;


	DMAx->CCR =
			DMA_CCR_MINC    |
			DMA_CCR_CIRC    |
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


uint8_t DMA_IsReady(DMA_Channel_TypeDef *DMAx){

	DMA_TypeDef *DMA = DMA_GetController(DMAx);
	uint8_t index = DMA_GetChannelIndex(DMAx);

	if(DMA == NULL || index == 0xFFu){
		return 0u;
	}

	uint32_t tc_mask =
		DMA_ISR_TCIF1 << (index * 4u);


	if((DMA->ISR & tc_mask) == 0u){
		return 0u;
	}


	DMA->IFCR =
		DMA_IFCR_CTCIF1 << (index * 4u);


	return 1u;
}
#endif
