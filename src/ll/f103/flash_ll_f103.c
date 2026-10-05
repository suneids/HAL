#include "../../../inc/flash_ll.h"
#if defined(STM32F103C6Tx)

static int flash_wait_ready(){
	// Wait for BSY reset
	while(FLASH->SR & FLASH_SR_BSY){}

	if(FLASH->SR & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)){
		return -1;
	}
	return 0;
}


static void flash_clear_flags(void){
	FLASH->SR |= FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR;
}


int flash_ll_unlock(){
	if((FLASH->CR & FLASH_CR_LOCK) == 0u){
		return 0;
	}

	FLASH->KEYR = FLASH_KEY1;
	FLASH->KEYR = FLASH_KEY2;

	if(FLASH->CR & FLASH_CR_LOCK){
		return -1;
	}
	return 0;
}


int flash_ll_lock(){
	FLASH->CR |= FLASH_CR_LOCK;
	return 0;
}


int flash_ll_erase_page(uint32_t page_addr){
	if(page_addr & (NV_PAGE_SIZE - 1u)){
		return -1;
	}

	__disable_irq();
	flash_clear_flags();
	if(flash_wait_ready() != 0){
		__enable_irq();
		return -1;
	}

	flash_clear_flags();

	FLASH->CR |= FLASH_CR_PER;
	FLASH->AR = page_addr;
	FLASH->CR |= FLASH_CR_STRT;

	int r = flash_wait_ready();
	FLASH->CR &= ~FLASH_CR_PER;

	flash_clear_flags();

	__enable_irq();

	return r;
}


int flash_ll_program_halfword(uint32_t addr, uint16_t data){
	int ret = -1;
	if(addr & 0x1u){
		return ret;
	}

	__disable_irq();
	do{
		flash_clear_flags();
		if(flash_wait_ready() != 0) break;

		flash_clear_flags();
		FLASH->CR |= FLASH_CR_PG;
		*(volatile uint16_t*)addr = data;
		int r = flash_wait_ready();
		FLASH->CR &= ~FLASH_CR_PG;
		flash_clear_flags();

		if(r != 0) break;
		if(*(volatile uint16_t*)addr != data) break;
		ret = 0;
	}while(0);
	__enable_irq();

	return ret;
}

int flash_ll_read_halfword(uint32_t addr, uint16_t *out){
	if(!out) return -1;
	if(addr & 0x1u) return -1;
	*out = *(volatile uint16_t*)addr;
	return 0;
}

#endif
