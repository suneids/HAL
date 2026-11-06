#include "../../../inc/dma.h"
#if defined(STM32F103C6Tx)

void DMA_Init(DMA_Channel_TypeDef *DMAx, uint32_t peripheral_addr, uint32_t memory_addr, uint16_t data_count){
	RCC->AHBENR |= RCC_AHBENR_DMA1EN;
	DMAx->CCR &= ~DMA_CCR_EN;
	DMAx->CCR = 0;
	DMAx->CCR |= DMA_CCR_MINC | DMA_CCR_CIRC | DMA_CCR_MSIZE_0 | DMA_CCR_PSIZE_0;

	DMAx->CPAR = peripheral_addr;
	DMAx->CMAR = memory_addr;
	DMAx->CNDTR = data_count;

	DMAx->CCR |= DMA_CCR_EN;

}


void SPI1_DMA_TX_Init(void)
{
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;

    DMA1_Channel3->CCR &= ~DMA_CCR_EN;
    DMA1_Channel3->CCR = 0;

    DMA1_Channel3->CPAR = (uint32_t)&SPI1->DR;

    DMA1_Channel3->CCR =
          DMA_CCR_DIR      // memory -> peripheral
        | DMA_CCR_MINC     // increment memory
        | DMA_CCR_TCIE     // interrupt on complete
        | DMA_CCR_PL_1;    // high priority

    NVIC_EnableIRQ(DMA1_Channel3_IRQn);
}


void SPI1_DMA_TX_Start(const uint8_t *buf, uint16_t len){
	if (!buf || len == 0u) {
		return;
	}

	DMA1_Channel3->CCR &= ~DMA_CCR_EN;

	DMA1->IFCR = DMA_IFCR_CGIF3 |
				 DMA_IFCR_CTCIF3 |
				 DMA_IFCR_CHTIF3 |
				 DMA_IFCR_CTEIF3;

	DMA1_Channel3->CMAR  = (uint32_t)buf;
	DMA1_Channel3->CNDTR = len;

	SPI1->CR2 |= SPI_CR2_TXDMAEN;
	DMA1_Channel3->CCR |= DMA_CCR_EN;
}


void SPI1_DMA_TX_Stop(){
   DMA1_Channel3->CCR &= ~DMA_CCR_EN;
   SPI1->CR2 &= ~SPI_CR2_TXDMAEN;
}

#endif
