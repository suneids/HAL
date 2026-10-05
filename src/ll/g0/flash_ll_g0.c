#include "../../../inc/flash_ll.h"
#include <stdint.h>
#if defined(STM32G0B1xx)
#define FLASH_LL_KEY1  0x45670123UL
#define FLASH_LL_KEY2  0xCDEF89ABUL
#define FLASH_LL_PAGE_SIZE  2048u

#define FLASH_LL_ERROR_MASK ( \
	FLASH_SR_OPERR   |          \
	FLASH_SR_PROGERR |          \
	FLASH_SR_WRPERR  |          \
	FLASH_SR_PGAERR  |          \
	FLASH_SR_SIZERR  |          \
	FLASH_SR_PGSERR  |          \
	FLASH_SR_MISERR  |          \
	FLASH_SR_FASTERR |          \
	FLASH_SR_RDERR              \
)


static int flash_wait_ready(void){
	/*
	 * В заголовочнике G0B1 определены оба флага,
	 * даже если в конкретном корпусе используется только Bank 1.
	 */
	while((FLASH->SR & (FLASH_SR_BSY1 | FLASH_SR_BSY2)) != 0u){
	}

	/* Ожидание завершения изменения управляющего регистра */
	while((FLASH->SR & FLASH_SR_CFGBSY) != 0u){
	}

	if((FLASH->SR & FLASH_LL_ERROR_MASK) != 0u){
		return -1;
	}

	return 0;
}


static void flash_clear_flags(void){
	/*
	 * Флаги очищаются записью единицы.
	 *
	 * Здесь нужна прямая запись, а не |=.
	 */
	FLASH->SR =
		  FLASH_SR_EOP
		| FLASH_LL_ERROR_MASK;
}


int flash_ll_unlock(void){
	if((FLASH->CR & FLASH_CR_LOCK) == 0u){
		return 0;
	}

	FLASH->KEYR = FLASH_LL_KEY1;
	FLASH->KEYR = FLASH_LL_KEY2;

	if((FLASH->CR & FLASH_CR_LOCK) != 0u){
		return -1;
	}

	return 0;
}


int flash_ll_lock(void){
	if(flash_wait_ready() != 0){
		return -1;
	}

	FLASH->CR |= FLASH_CR_LOCK;

	return ((FLASH->CR & FLASH_CR_LOCK) != 0u) ? 0 : -1;
}


int flash_ll_erase_page(uint32_t page_addr){
	/* Проверка попадания во Flash */
	if(page_addr < FLASH_BASE){
		return -1;
	}

	if(page_addr >= (FLASH_BASE + FLASH_SIZE)){
		return -1;
	}

	/* Адрес должен указывать на начало страницы 2 Кбайт */
	if(((page_addr - FLASH_BASE) % FLASH_LL_PAGE_SIZE) != 0u){
		return -1;
	}

	/*
	 * Для STM32G0B1CBT6:
	 *
	 * Flash = 128 Кбайт
	 * Page  = 2 Кбайт
	 * Pages = 64
	 * Номера страниц: 0...63
	 */
	uint32_t page =
		(page_addr - FLASH_BASE) / FLASH_LL_PAGE_SIZE;

	uint32_t primask = __get_PRIMASK();
	__disable_irq();

	int result = -1;

	do{
		if(flash_wait_ready() != 0){
			break;
		}

		flash_clear_flags();

		/*
		 * Стереть старую конфигурацию операции.
		 *
		 * BKER = 0: Bank 1.
		 */
		FLASH->CR &= ~(FLASH_CR_PER |
		               FLASH_CR_PNB |
		               FLASH_CR_BKER);

		/* Выбрать страницу и режим page erase */
		FLASH->CR |=
			  FLASH_CR_PER
			| (page << FLASH_CR_PNB_Pos);

		/* Запустить стирание */
		FLASH->CR |= FLASH_CR_STRT;

		if(flash_wait_ready() != 0){
			break;
		}

		result = 0;
	}
	while(0);

	/* Убрать настройки стирания */
	FLASH->CR &= ~(FLASH_CR_PER |
	               FLASH_CR_PNB |
	               FLASH_CR_BKER);

	flash_clear_flags();

	/* Восстановить прежнее состояние IRQ */
	__set_PRIMASK(primask);

	return result;
}


/*
 * STM32G0 записывает обычным режимом только doubleword:
 * 64 бита, адрес кратен восьми.
 */
int flash_ll_program_doubleword(uint32_t addr, uint64_t data){
	if((addr & 0x7u) != 0u){
		return -1;
	}

	if(addr < FLASH_BASE){
		return -1;
	}

	if(addr > (FLASH_BASE + FLASH_SIZE - 8u)){
		return -1;
	}

	uint32_t primask = __get_PRIMASK();
	__disable_irq();

	int result = -1;

	do{
		if(flash_wait_ready() != 0){
			break;
		}

		flash_clear_flags();

		FLASH->CR |= FLASH_CR_PG;

		/*
		 * Порядок обязателен:
		 * сначала младшие 32 бита,
		 * затем старшие 32 бита.
		 */
		*(volatile uint32_t *)addr =
			(uint32_t)data;

		__ISB();

		*(volatile uint32_t *)(addr + 4u) =
			(uint32_t)(data >> 32u);

		if(flash_wait_ready() != 0){
			break;
		}

		uint32_t read_low =
			*(volatile const uint32_t *)addr;

		uint32_t read_high =
			*(volatile const uint32_t *)(addr + 4u);

		if(read_low != (uint32_t)data){
			break;
		}

		if(read_high != (uint32_t)(data >> 32u)){
			break;
		}

		result = 0;
	}
	while(0);

	FLASH->CR &= ~FLASH_CR_PG;

	flash_clear_flags();

	__set_PRIMASK(primask);

	return result;
}


int flash_ll_read_doubleword(uint32_t addr, uint64_t *out){
	if(out == NULL){
		return -1;
	}

	if((addr & 0x7u) != 0u){
		return -1;
	}

	if(addr < FLASH_BASE ||
	   addr > (FLASH_BASE + FLASH_SIZE - 8u)){
		return -1;
	}

	uint32_t low =
		*(volatile const uint32_t *)addr;

	uint32_t high =
		*(volatile const uint32_t *)(addr + 4u);

	*out = ((uint64_t)high << 32u) | low;

	return 0;
}


/*
 * Читать halfword по-прежнему можно.
 * Ограничение 64 бит касается программирования, а не чтения.
 */
int flash_ll_read_halfword(uint32_t addr, uint16_t *out){
	if(out == NULL){
		return -1;
	}

	if((addr & 0x1u) != 0u){
		return -1;
	}

	if(addr < FLASH_BASE ||
	   addr > (FLASH_BASE + FLASH_SIZE - 2u)){
		return -1;
	}

	*out = *(volatile const uint16_t *)addr;

	return 0;
}

#endif
