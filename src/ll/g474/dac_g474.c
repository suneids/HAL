#include "../../../inc/dac.h"
#include "../../../inc/tim.h"

#if defined(STM32G474xx)

void DAC_Init(void){

    RCC->AHB2ENR |= RCC_AHB2ENR_DAC1EN;

    RCC->AHB2RSTR |= RCC_AHB2RSTR_DAC1RST;
    RCC->AHB2RSTR &= ~RCC_AHB2RSTR_DAC1RST;


    DAC1->CR &= ~(DAC_CR_EN1 | DAC_CR_EN2);


    /*
     * MODE = 000:
     * normal mode
     * external output
     * output buffer enabled
     */
    DAC1->MCR &= ~(DAC_MCR_MODE1 | DAC_MCR_MODE2 | DAC_MCR_HFSEL);


    uint32_t hclk = RCC_GetHclk_Hz();

    if(hclk > 160000000u){
        DAC1->MCR |= DAC_MCR_HFSEL_1;
    }
    else if(hclk > 80000000u){
        DAC1->MCR |= DAC_MCR_HFSEL_0;
    }


    // Без триггеров
    DAC1->CR &= ~(DAC_CR_TEN1 | DAC_CR_TEN2);


    DAC1->DHR12R1 = 0;
    DAC1->DHR12R2 = 0;


    DAC1->CR |= DAC_CR_EN1 | DAC_CR_EN2;
}


void DAC_Write(DAC_Channel_t channel, uint16_t value){

    if(value > 4095u) value = 4095u;

    if(channel == DAC_CHANNEL_1){
        DAC1->DHR12R1 = value;
    }
    else if(channel == DAC_CHANNEL_2){
        DAC1->DHR12R2 = value;
    }
}


void DAC_WriteDual(uint16_t channel1, uint16_t channel2){

    if(channel1 > 4095u) channel1 = 4095u;
    if(channel2 > 4095u) channel2 = 4095u;

    DAC1->DHR12RD = ((uint32_t)channel2 << DAC_DHR12RD_DACC2DHR_Pos) |  ((uint32_t)channel1 << DAC_DHR12RD_DACC1DHR_Pos);
}

#endif
