#ifndef FLASH_LL_F103C6T6
#define FLASH_LL_F103C6T6
#include "mcu_config.h"
#include <stddef.h>
#include <stdint.h>

int flash_ll_unlock();
int flash_ll_lock();
int flash_ll_erase_page(uint32_t page_addr);

#if defined(STM32F103C6Tx)

int flash_ll_read_halfword(uint32_t addr, uint16_t *out);
int flash_ll_program_halfword(uint32_t addr, uint16_t data);

#elif defined(STM32G0B1CBTx)

int flash_ll_program_doubleword(
	uint32_t addr,
	uint64_t data
);

int flash_ll_read_doubleword(
	uint32_t addr,
	uint64_t *out
);

int flash_ll_read_halfword(
	uint32_t addr,
	uint16_t *out
);

#endif
#endif
