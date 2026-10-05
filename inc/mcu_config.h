#ifndef MCU_CONFIG
#define MCU_CONFIG
#include <stdint.h>



#if defined(STM32F103C6Tx)

	#define NV_PAGE_SIZE 1024u
	#define SYS_CLK_HZ 72000000
	#define HSI_VALUE 8000000u
	#define HSE_VALUE 8000000u
	#define NV_PAGE_SIZE 1024u
	#define STM32F103x6 1
	#include "../ST/f103/stm32f1xx.h"


#elif defined(STM32G0B1CBTx)

	#define HSI_VALUE 16000000UL
	#define HSE_VALUE 8000000UL
	#define LSI_VALUE 32000UL
	#define LSE_VALUE 32768UL
	#define NV_PAGE_SIZE 2048u
    #define STM32G0B1xx 1
	#include "../ST/g0/stm32g0xx.h"


#else

  #error Unsupported STM32 target

#endif

#endif
