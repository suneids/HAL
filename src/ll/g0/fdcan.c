#include "../../../inc/fdcan.h"
#include "../../../inc/tim.h"   // пока только ради RCC_GetPclk1_Hz()
#include "../../../inc/gpio.h"

#if defined(STM32G0B1xx)

#define FDCAN_TQ_PER_BIT       16u
#define FDCAN_NTSEG1           13u
#define FDCAN_NTSEG2           2u
#define FDCAN_NSJW             2u

#define FDCAN_TIMEOUT          100000u


// ---------- Message RAM layout STM32G0 ----------

#define SRAMCAN_FLS_NBR        28u
#define SRAMCAN_FLE_NBR        8u
#define SRAMCAN_RF0_NBR        3u
#define SRAMCAN_RF1_NBR        3u
#define SRAMCAN_TEF_NBR        3u
#define SRAMCAN_TFQ_NBR        3u

#define SRAMCAN_FLS_SIZE       (1u  * 4u)
#define SRAMCAN_FLE_SIZE       (2u  * 4u)
#define SRAMCAN_RF0_SIZE       (18u * 4u)
#define SRAMCAN_RF1_SIZE       (18u * 4u)
#define SRAMCAN_TEF_SIZE       (2u  * 4u)
#define SRAMCAN_TFQ_SIZE       (18u * 4u)

#define SRAMCAN_FLSSA          0u
#define SRAMCAN_FLESA          (SRAMCAN_FLSSA + SRAMCAN_FLS_NBR * SRAMCAN_FLS_SIZE)
#define SRAMCAN_RF0SA          (SRAMCAN_FLESA + SRAMCAN_FLE_NBR * SRAMCAN_FLE_SIZE)
#define SRAMCAN_RF1SA          (SRAMCAN_RF0SA + SRAMCAN_RF0_NBR * SRAMCAN_RF0_SIZE)
#define SRAMCAN_TEFSA          (SRAMCAN_RF1SA + SRAMCAN_RF1_NBR * SRAMCAN_RF1_SIZE)
#define SRAMCAN_TFQSA          (SRAMCAN_TEFSA + SRAMCAN_TEF_NBR * SRAMCAN_TEF_SIZE)

#define SRAMCAN_SIZE           (SRAMCAN_TFQSA + SRAMCAN_TFQ_NBR * SRAMCAN_TFQ_SIZE)


// ---------- private ----------
static uint8_t FDCAN_PinInit(FDCAN_GlobalTypeDef *FDCANx)
{
    GPIO_Pin_t rx = {0};
    GPIO_Pin_t tx = {0};

    if(FDCANx == FDCAN1){

        rx = (GPIO_Pin_t){
            .port   = GPIOB,
            .number = 8,
            .pull   = 0,
            .af     = 3,
            .moder  = GPIO_MODE_AF,
            .otype  = GPIO_OTYPE_PP,
            .speed  = GPIO_SPEED_HIGH
        };

        tx = (GPIO_Pin_t){
            .port   = GPIOB,
            .number = 9,
            .pull   = 0,
            .af     = 3,
            .moder  = GPIO_MODE_AF,
            .otype  = GPIO_OTYPE_PP,
            .speed  = GPIO_SPEED_HIGH
        };

    }
#ifdef FDCAN2
    else if(FDCANx == FDCAN2){

        rx = (GPIO_Pin_t){
            .port   = GPIOB,
            .number = 5,
            .pull   = 0,
            .af     = 3,
            .moder  = GPIO_MODE_AF,
            .otype  = GPIO_OTYPE_PP,
            .speed  = GPIO_SPEED_HIGH
        };

        tx = (GPIO_Pin_t){
            .port   = GPIOB,
            .number = 6,
            .pull   = 0,
            .af     = 3,
            .moder  = GPIO_MODE_AF,
            .otype  = GPIO_OTYPE_PP,
            .speed  = GPIO_SPEED_HIGH
        };

    }
#endif
    else{
        return 0;
    }

    GPIO_PinMode(rx);
    GPIO_PinMode(tx);

    return 1;
}


static uint32_t FDCAN_GetRamBase(FDCAN_GlobalTypeDef *FDCANx){

    uint32_t base = SRAMCAN_BASE;

#ifdef FDCAN2
    if(FDCANx == FDCAN2){
        base += SRAMCAN_SIZE;
    }
#endif

    return base;
}


static uint8_t FDCAN_WaitSet(volatile uint32_t *reg, uint32_t mask){

    uint32_t timeout = FDCAN_TIMEOUT;

    while(((*reg & mask) == 0u) && timeout){
        timeout--;
    }

    return timeout != 0u;
}


static uint8_t FDCAN_WaitClear(volatile uint32_t *reg, uint32_t mask){

    uint32_t timeout = FDCAN_TIMEOUT;

    while(((*reg & mask) != 0u) && timeout){
        timeout--;
    }

    return timeout != 0u;
}


static uint8_t FDCAN_IsValid(FDCAN_GlobalTypeDef *FDCANx)
{
    if(FDCANx == FDCAN1) return 1;

#ifdef FDCAN2
    if(FDCANx == FDCAN2) return 1;
#endif

    return 0;
}

// ---------- INIT ----------

uint8_t FDCAN_Init(FDCAN_GlobalTypeDef *FDCANx, uint32_t bitrate){
    if(!FDCAN_IsValid(FDCANx)) return 0;
    if(bitrate == 0u)          return 0;

    if(!FDCAN_PinInit(FDCANx)) return 0;

    /*
     * FDCAN kernel clock = PCLK1.
     *
     * FDCANSEL:
     * 00 -> PCLK1
     * 01 -> PLLQ
     * 10 -> HSE
     */
    RCC->CCIPR2 &= ~RCC_CCIPR2_FDCANSEL;

    RCC->APBENR1 |= RCC_APBENR1_FDCANEN;


    uint32_t fdcan_clk = RCC_GetPclk1_Hz();

    uint32_t divider = bitrate * FDCAN_TQ_PER_BIT;

    if(divider == 0u){
        return 0;
    }

    /*
     * Хотим:
     *
     * bitrate = FDCAN_CLK /
     *           (prescaler * (1 + NTSEG1 + NTSEG2))
     *
     * 1 + 13 + 2 = 16 tq
     */

    if((fdcan_clk % divider) != 0u){
        return 0;
    }

    uint32_t prescaler = fdcan_clk / divider;

    if(prescaler < 1u || prescaler > 512u){
        return 0;
    }


    // Exit clock stop / sleep
    FDCANx->CCCR &= ~FDCAN_CCCR_CSR;

    if(!FDCAN_WaitClear(&FDCANx->CCCR, FDCAN_CCCR_CSA)){
        return 0;
    }


    // Enter initialization mode
    FDCANx->CCCR |= FDCAN_CCCR_INIT;

    if(!FDCAN_WaitSet(&FDCANx->CCCR, FDCAN_CCCR_INIT)){
        return 0;
    }


    // Allow configuration changes
    FDCANx->CCCR |= FDCAN_CCCR_CCE;


    /*
     * CKDIV = 0 -> /1
     * Divider common for FDCAN peripherals.
     */
    if(FDCANx == FDCAN1){
        FDCAN_CONFIG->CKDIV = 0u;
    }


    /*
     * Classic CAN
     * Normal mode
     * Auto retransmission ON
     * No BRS
     */
    FDCANx->CCCR &= ~(FDCAN_CCCR_FDOE |
                      FDCAN_CCCR_BRSE |
                      FDCAN_CCCR_TEST |
                      FDCAN_CCCR_MON  |
                      FDCAN_CCCR_ASM  |
                      FDCAN_CCCR_DAR  |
                      FDCAN_CCCR_TXP);

    FDCANx->TEST &= ~FDCAN_TEST_LBCK;


    /*
     * Nominal bit timing:
     *
     * Sync  = 1 tq
     * Seg1  = 13 tq
     * Seg2  = 2 tq
     * SJW   = 2 tq
     *
     * Sample point = 14 / 16 = 87.5%
     */
    FDCANx->NBTP =
        ((FDCAN_NSJW   - 1u) << FDCAN_NBTP_NSJW_Pos)   |
        ((FDCAN_NTSEG1 - 1u) << FDCAN_NBTP_NTSEG1_Pos) |
        ((FDCAN_NTSEG2 - 1u) << FDCAN_NBTP_NTSEG2_Pos) |
        ((prescaler    - 1u) << FDCAN_NBTP_NBRP_Pos);


    // TX FIFO, not priority queue
    FDCANx->TXBC &= ~FDCAN_TXBC_TFQM;


    /*
     * Пока фильтров нет:
     *
     * Standard ID, не совпавшие с фильтрами -> FIFO0
     * Extended -> reject
     * Remote frames -> reject
     *
     * LSS = 0
     * LSE = 0
     */
    FDCANx->RXGFC &=
        ~(FDCAN_RXGFC_ANFS |
          FDCAN_RXGFC_ANFE |
          FDCAN_RXGFC_RRFS |
          FDCAN_RXGFC_RRFE |
          FDCAN_RXGFC_LSS  |
          FDCAN_RXGFC_LSE  |
          FDCAN_RXGFC_F0OM);

    // ANFS = 00 -> non-matching standard -> FIFO0

    // ANFE = 10 -> reject extended
    FDCANx->RXGFC |= (2u << FDCAN_RXGFC_ANFE_Pos);

    // reject remote standard + extended
    FDCANx->RXGFC |= FDCAN_RXGFC_RRFS |
                     FDCAN_RXGFC_RRFE;


    // Clear this instance Message RAM
    uint32_t ram_base = FDCAN_GetRamBase(FDCANx);

    for(uint32_t address = ram_base;
        address < (ram_base + SRAMCAN_SIZE);
        address += 4u){

        *((volatile uint32_t *)address) = 0u;
    }


    // Пока без interrupts
    FDCANx->IE  = 0u;
    FDCANx->ILE = 0u;

    // Interrupt flags are W1C
    FDCANx->IR = 0xFFFFFFFFu;


    // Start FDCAN
    FDCANx->CCCR &= ~FDCAN_CCCR_INIT;

    if(!FDCAN_WaitClear(&FDCANx->CCCR, FDCAN_CCCR_INIT)){
        return 0;
    }


    return 1;
}



// ---------- SEND ----------

uint8_t FDCAN_Send(FDCAN_GlobalTypeDef *FDCANx,
                   uint32_t id,
                   const uint8_t *data,
                   uint8_t len){
	if(!FDCAN_IsValid(FDCANx))   return 0;
    if(id > 0x7FFu) 			 return 0;
    if(len > 8u) 				 return 0;
    if(len > 0u && data == NULL) return 0;



    // TX FIFO full
    if(FDCANx->TXFQS & FDCAN_TXFQS_TFQF) return 0;

    uint32_t put_index =
        (FDCANx->TXFQS & FDCAN_TXFQS_TFQPI)
        >> FDCAN_TXFQS_TFQPI_Pos;


    uint32_t ram_base = FDCAN_GetRamBase(FDCANx);

    volatile uint32_t *tx =
        (volatile uint32_t *)
        (ram_base +
         SRAMCAN_TFQSA +
         put_index * SRAMCAN_TFQ_SIZE);


    /*
     * Tx element W0:
     *
     * Standard ID bits 28:18
     * XTD = 0
     * RTR = 0
     * ESI = 0
     */
    tx[0] = id << 18u;


    /*
     * Tx element W1:
     *
     * DLC bits 19:16
     * BRS = 0
     * FDF = 0
     * EFC = 0
     */
    tx[1] = ((uint32_t)len << 16u);


    uint32_t data0 = 0u;
    uint32_t data1 = 0u;

    for(uint8_t i = 0; i < len; i++){

        if(i < 4u){
            data0 |= ((uint32_t)data[i] << (i * 8u));
        }
        else{
            data1 |= ((uint32_t)data[i] << ((i - 4u) * 8u));
        }
    }

    tx[2] = data0;
    tx[3] = data1;

    __DSB();


    // Request transmission for this FIFO element
    FDCANx->TXBAR = (1u << put_index);


    return 1;
}



// ---------- READ ----------

uint8_t FDCAN_Read(FDCAN_GlobalTypeDef *FDCANx,
                   uint32_t *id,
                   uint8_t *data,
                   uint8_t *len){
	if(!FDCAN_IsValid(FDCANx)) return 0;
    if(id == NULL ||
       data == NULL ||
       len == NULL){

        return 0;
    }


    // RX FIFO0 empty
    if((FDCANx->RXF0S & FDCAN_RXF0S_F0FL) == 0u){
        return 0;
    }


    uint32_t get_index =
        (FDCANx->RXF0S & FDCAN_RXF0S_F0GI)
        >> FDCAN_RXF0S_F0GI_Pos;


    uint32_t ram_base = FDCAN_GetRamBase(FDCANx);

    volatile uint32_t *rx =
        (volatile uint32_t *)
        (ram_base +
         SRAMCAN_RF0SA +
         get_index * SRAMCAN_RF0_SIZE);


    uint32_t word0 = rx[0];
    uint32_t word1 = rx[1];


    // XTD set -> extended frame, мы его сейчас не поддерживаем
    if(word0 & (1u << 30u)){

        FDCANx->RXF0A = get_index;
        return 0;
    }


    // RTR set -> remote frame
    if(word0 & (1u << 29u)){

        FDCANx->RXF0A = get_index;
        return 0;
    }


    *id = (word0 >> 18u) & 0x7FFu;


    uint8_t dlc =
        (uint8_t)((word1 >> 16u) & 0x0Fu);


    if(dlc > 8u){

        FDCANx->RXF0A = get_index;
        return 0;
    }


    *len = dlc;


    uint32_t data0 = rx[2];
    uint32_t data1 = rx[3];

    for(uint8_t i = 0; i < dlc; i++){

        if(i < 4u){
            data[i] = (uint8_t)(data0 >> (i * 8u));
        }
        else{
            data[i] = (uint8_t)(data1 >> ((i - 4u) * 8u));
        }
    }


    // Release oldest FIFO0 element
    FDCANx->RXF0A = get_index;


    return 1;
}

#endif
