#include "../../../inc/dac.h"

#if defined(STM32G0B1xx)

void DAC_Init(void) {
    // Включаем тактирование DAC1
    RCC->APBENR1 |= RCC_APBENR1_DAC1EN;

    // Сбрасываем периферию в известное состояние
    RCC->APBRSTR1 |= RCC_APBRSTR1_DAC1RST;
    RCC->APBRSTR1 &= ~RCC_APBRSTR1_DAC1RST;

    // Пока отключаем оба канала
    DAC1->CR &= ~(DAC_CR_EN1 | DAC_CR_EN2);

    /*
     * MODE = 000:
     * normal mode,
     * выход подключён к внешнему пину,
     * внутренний выходной буфер включён.
     */
    DAC1->MCR &= ~(DAC_MCR_MODE1 | DAC_MCR_MODE2);

    // Работа без триггеров: значение обновляется обычной записью
    DAC1->CR &= ~(DAC_CR_TEN1 | DAC_CR_TEN2);

    // Начальное значение 0
    DAC1->DHR12R1 = 0;
    DAC1->DHR12R2 = 0;

    // Включаем оба канала
    DAC1->CR |= DAC_CR_EN1 | DAC_CR_EN2;
}


void DAC_Write(DAC_Channel_t channel, uint16_t value) {
    if(value > 4095) value = 4095;

    if(channel == DAC_CHANNEL_1) {
        DAC1->DHR12R1 = value;
    }
    else if(channel == DAC_CHANNEL_2) {
        DAC1->DHR12R2 = value;
    }
}


void DAC_WriteDual(uint16_t channel1, uint16_t channel2) {
    if(channel1 > 4095) channel1 = 4095;
    if(channel2 > 4095) channel2 = 4095;

    DAC1->DHR12RD = ((uint32_t)channel2 << DAC_DHR12RD_DACC2DHR_Pos) | ((uint32_t)channel1 << DAC_DHR12RD_DACC1DHR_Pos);
}



void DAC_SetTrigger(DAC_Channel_t channel, DAC_Trigger_t trigger)
{
    uint32_t shift;

    if(channel == DAC_CHANNEL_1)      shift = 0;
    else if(channel == DAC_CHANNEL_2) shift = 16;
    else return;

    uint32_t en = DAC_CR_EN1 << shift;
    uint32_t was_enabled = DAC1->CR & en;

    DAC1->CR &= ~en;

    DAC1->CR &= ~((DAC_CR_TSEL1 | DAC_CR_TEN1) << shift);

    if(trigger != DAC_TRIGGER_NONE){
        DAC1->CR |= ((uint32_t)trigger << (DAC_CR_TSEL1_Pos + shift))| (DAC_CR_TEN1 << shift);
    }

    if(was_enabled) DAC1->CR |= en;
}

void DAC_EnableDMA(DAC_Channel_t channel)
{
    if(channel == DAC_CHANNEL_1)
        DAC1->CR |= DAC_CR_DMAEN1;

    else if(channel == DAC_CHANNEL_2)
        DAC1->CR |= DAC_CR_DMAEN2;
}

#endif
