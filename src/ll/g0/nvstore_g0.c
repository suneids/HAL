#include "../../../inc/mcu_config.h"
#if defined(STM32G0B1xx)
#include "../../../inc/nvstore.h"
#include "../../../inc/flash_ll.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>





/*
 * STM32G0B1:
 *
 * Flash page size       = 2048 bytes
 * Programming granule   = 8 bytes
 */
#define NV_FLASH_PAGE_SIZE   2048u
#define NV_PROGRAM_SIZE      8u

#define NV_MAX_PAYLOAD       256u

/*
 * Записывается последним вместе с CRC32
 * в одном 64-битном doubleword.
 */
#define NV_COMMIT_MAGIC      0xC35AA53Cu


/*
 * Размер заголовка должен быть кратен 8.
 *
 * Физическая структура записи:
 *
 * [ NV_HDR_t                ]
 * [ payload                 ]
 * [ padding 0xFF до 8 байт  ]
 * [ NV_TAIL_t               ]
 */
typedef struct
{
	uint32_t magic;
	uint32_t seq;

	uint16_t version;
	uint16_t payload_len;

	uint32_t reserved;
} NV_HDR_t;


typedef struct
{
	uint32_t crc32;
	uint32_t commit;
} NV_TAIL_t;


_Static_assert(
	sizeof(NV_HDR_t) == 16u,
	"NV_HDR_t must be exactly 16 bytes"
);

_Static_assert(
	sizeof(NV_TAIL_t) == 8u,
	"NV_TAIL_t must be exactly 8 bytes"
);


/* ------------------------------------------------------------------------- */
/* Вспомогательные функции                                                   */
/* ------------------------------------------------------------------------- */


static uint32_t align8(uint32_t value)
{
	return (value + 7u) & ~7u;
}


static uint32_t recordTailOffset(uint16_t payload_len)
{
	return align8(
		(uint32_t)sizeof(NV_HDR_t) +
		(uint32_t)payload_len
	);
}


static uint32_t recordSize(uint16_t payload_len)
{
	return
		recordTailOffset(payload_len) +
		(uint32_t)sizeof(NV_TAIL_t);
}


static uint32_t recordPayloadAddress(uint32_t record_addr)
{
	return record_addr + (uint32_t)sizeof(NV_HDR_t);
}


static uint32_t recordTailAddress(
	uint32_t record_addr,
	uint16_t payload_len)
{
	return record_addr + recordTailOffset(payload_len);
}


static uint32_t otherPage(
	const NVStore_t *store,
	uint32_t page)
{
	return
		(page == store->cfg.page_a)
		? store->cfg.page_b
		: store->cfg.page_a;
}


/*
 * Обрабатывает переполнение uint32_t sequence.
 *
 * Корректно, пока между сравниваемыми sequence
 * не прошло более 2^31 записей.
 */
static bool sequenceIsNewer(
	uint32_t candidate,
	uint32_t current)
{
	return (int32_t)(candidate - current) > 0;
}


/* ------------------------------------------------------------------------- */
/* CRC32                                                                     */
/* ------------------------------------------------------------------------- */


static uint32_t crc32Update(
	uint32_t crc,
	const void *data,
	uint32_t len)
{
	const uint8_t *bytes =
		(const uint8_t *)data;

	for(uint32_t i = 0u; i < len; i++)
	{
		crc ^= bytes[i];

		for(uint32_t bit = 0u; bit < 8u; bit++)
		{
			uint32_t mask =
				(uint32_t)-(int32_t)(crc & 1u);

			crc =
				(crc >> 1u) ^
				(0xEDB88320u & mask);
		}
	}

	return crc;
}


/*
 * CRC считается по заголовку и payload.
 *
 * Таким образом, CRC защищает:
 * - magic;
 * - seq;
 * - version;
 * - payload_len;
 * - reserved;
 * - непосредственно payload.
 */
static uint32_t recordCRC(
	const NV_HDR_t *header,
	const void *payload,
	uint32_t payload_len)
{
	uint32_t crc = UINT32_MAX;

	crc = crc32Update(
		crc,
		header,
		(uint32_t)sizeof(NV_HDR_t)
	);

	crc = crc32Update(
		crc,
		payload,
		payload_len
	);

	return ~crc;
}


/* ------------------------------------------------------------------------- */
/* Чтение Flash                                                              */
/* ------------------------------------------------------------------------- */


static void readHeader(
	uint32_t address,
	NV_HDR_t *header)
{
	memcpy(
		header,
		(const void *)(uintptr_t)address,
		sizeof(*header)
	);
}


static void readTail(
	uint32_t address,
	NV_TAIL_t *tail)
{
	memcpy(
		tail,
		(const void *)(uintptr_t)address,
		sizeof(*tail)
	);
}


/*
 * Проверяет, что весь слот остаётся в состоянии erase:
 * каждый байт равен 0xFF.
 *
 * Проверяем uint32_t, чтобы не требовать от ядра
 * нативной 64-битной загрузки.
 */
static bool slotIsErased(
	const NVStore_t *store,
	uint32_t record_addr)
{
	for(uint32_t offset = 0u;
	    offset < store->step;
	    offset += 4u)
	{
		uint32_t value =
			*(volatile const uint32_t *)
			(uintptr_t)(record_addr + offset);

		if(value != UINT32_MAX)
		{
			return false;
		}
	}

	return true;
}


/* ------------------------------------------------------------------------- */
/* Программирование Flash                                                    */
/* ------------------------------------------------------------------------- */


static NV_Status_t programBytes(
	uint32_t address,
	const void *data,
	uint32_t len)
{
	if(data == NULL && len != 0u)
	{
		return NV_BAD_PARAM;
	}

	if((address & 0x7u) != 0u)
	{
		return NV_BAD_PARAM;
	}

	const uint8_t *source =
		(const uint8_t *)data;

	for(uint32_t offset = 0u;
	    offset < len;
	    offset += NV_PROGRAM_SIZE)
	{
		/*
		 * Недостающие байты последнего блока
		 * остаются стёртыми: 0xFF.
		 */
		uint64_t value = UINT64_MAX;

		uint32_t remaining =
			len - offset;

		uint32_t chunk =
			remaining < NV_PROGRAM_SIZE
			? remaining
			: NV_PROGRAM_SIZE;

		memcpy(
			&value,
			source + offset,
			chunk
		);

		if(flash_ll_program_doubleword(
			address + offset,
			value) != 0)
		{
			return NV_IO_FAIL;
		}
	}

	return NV_OK;
}


/* ------------------------------------------------------------------------- */
/* Поиск записей                                                             */
/* ------------------------------------------------------------------------- */


static NV_Status_t scanPage(
	NVStore_t *store,
	uint32_t page_addr,
	uint32_t *out_best_addr,
	uint32_t *out_best_seq)
{
	if(store == NULL ||
	   out_best_addr == NULL ||
	   out_best_seq == NULL)
	{
		return NV_BAD_PARAM;
	}

	NV_CFG_t *cfg = &store->cfg;

	*out_best_addr = 0u;
	*out_best_seq = 0u;

	uint32_t page_end =
		page_addr + cfg->page_size;

	for(uint32_t record_addr = page_addr;
	    record_addr + store->step <= page_end;
	    record_addr += store->step)
	{
		/*
		 * Не используем break:
		 *
		 * после аварийно оборванной записи может остаться
		 * занятый невалидный слот, а следующие записи могут
		 * находиться дальше.
		 */
		if(slotIsErased(store, record_addr))
		{
			continue;
		}

		uint32_t tail_addr =
			recordTailAddress(
				record_addr,
				cfg->payload_len
			);

		NV_TAIL_t tail;
		readTail(tail_addr, &tail);

		/*
		 * Commit записывается последним.
		 *
		 * Если commit отсутствует, запись считается
		 * незавершённой и payload не читается.
		 */
		if(tail.commit != NV_COMMIT_MAGIC)
		{
			continue;
		}

		NV_HDR_t header;
		readHeader(record_addr, &header);

		if(header.magic != cfg->magic)
		{
			continue;
		}

		if(header.version != cfg->version)
		{
			continue;
		}

		if(header.payload_len != cfg->payload_len)
		{
			continue;
		}

		uint32_t payload_addr =
			recordPayloadAddress(record_addr);

		const void *payload =
			(const void *)(uintptr_t)payload_addr;

		uint32_t calculated_crc =
			recordCRC(
				&header,
				payload,
				cfg->payload_len
			);

		if(calculated_crc != tail.crc32)
		{
			continue;
		}

		if(*out_best_addr == 0u ||
		   sequenceIsNewer(
			   header.seq,
			   *out_best_seq))
		{
			*out_best_addr = record_addr;
			*out_best_seq = header.seq;
		}
	}

	return NV_OK;
}


static NV_Status_t findEmptySlot(
	const NVStore_t *store,
	uint32_t page_addr,
	uint32_t *out_addr)
{
	if(store == NULL || out_addr == NULL)
	{
		return NV_BAD_PARAM;
	}

	uint32_t page_end =
		page_addr + store->cfg.page_size;

	for(uint32_t record_addr = page_addr;
	    record_addr + store->step <= page_end;
	    record_addr += store->step)
	{
		if(slotIsErased(store, record_addr))
		{
			*out_addr = record_addr;
			return NV_OK;
		}
	}

	return NV_FULL;
}


/* ------------------------------------------------------------------------- */
/* Публичный API                                                             */
/* ------------------------------------------------------------------------- */


NV_Status_t NV_Init(NVStore_t *store)
{
	if(store == NULL)
	{
		return NV_BAD_PARAM;
	}

	NV_CFG_t *cfg = &store->cfg;

	if(cfg->page_size != NV_FLASH_PAGE_SIZE)
	{
		return NV_BAD_PARAM;
	}

	if(cfg->payload_len == 0u)
	{
		return NV_BAD_PARAM;
	}

	if(cfg->payload_len > NV_MAX_PAYLOAD)
	{
		return NV_BAD_PARAM;
	}

	if(cfg->page_a == cfg->page_b)
	{
		return NV_BAD_PARAM;
	}

	if(cfg->page_a < FLASH_BASE ||
	   cfg->page_b < FLASH_BASE)
	{
		return NV_BAD_PARAM;
	}

	/*
	 * Адреса должны быть выровнены относительно
	 * начала Flash, а не просто относительно нуля.
	 */
	if(((cfg->page_a - FLASH_BASE) %
	    cfg->page_size) != 0u)
	{
		return NV_BAD_PARAM;
	}

	if(((cfg->page_b - FLASH_BASE) %
	    cfg->page_size) != 0u)
	{
		return NV_BAD_PARAM;
	}

	store->step =
		recordSize(cfg->payload_len);

	if((store->step & 0x7u) != 0u)
	{
		return NV_BAD_PARAM;
	}

	if(store->step > cfg->page_size)
	{
		return NV_BAD_PARAM;
	}

	uint32_t page_a_addr = 0u;
	uint32_t page_b_addr = 0u;

	uint32_t page_a_seq = 0u;
	uint32_t page_b_seq = 0u;

	NV_Status_t status =
		scanPage(
			store,
			cfg->page_a,
			&page_a_addr,
			&page_a_seq
		);

	if(status != NV_OK)
	{
		return status;
	}

	status =
		scanPage(
			store,
			cfg->page_b,
			&page_b_addr,
			&page_b_seq
		);

	if(status != NV_OK)
	{
		return status;
	}

	/*
	 * Ни одной валидной записи нет.
	 *
	 * Страницы могут содержать:
	 * - мусор старой версии;
	 * - оборванную запись;
	 * - полностью стёртое состояние.
	 *
	 * Приводим обе страницы к гарантированно
	 * чистому состоянию.
	 */
	if(page_a_addr == 0u &&
	   page_b_addr == 0u)
	{
		if(flash_ll_unlock() != 0)
		{
			return NV_IO_FAIL;
		}

		if(flash_ll_erase_page(
			cfg->page_a) != 0)
		{
			(void)flash_ll_lock();
			return NV_IO_FAIL;
		}

		if(flash_ll_erase_page(
			cfg->page_b) != 0)
		{
			(void)flash_ll_lock();
			return NV_IO_FAIL;
		}

		if(flash_ll_lock() != 0)
		{
			return NV_IO_FAIL;
		}

		store->last_addr = 0u;
		store->last_seq = 0u;
		store->active_page = cfg->page_a;

		return NV_EMPTY;
	}

	/*
	 * Если валидна только одна страница,
	 * выбираем её.
	 */
	if(page_a_addr == 0u)
	{
		store->last_addr = page_b_addr;
		store->last_seq = page_b_seq;
		store->active_page = cfg->page_b;

		return NV_OK;
	}

	if(page_b_addr == 0u)
	{
		store->last_addr = page_a_addr;
		store->last_seq = page_a_seq;
		store->active_page = cfg->page_a;

		return NV_OK;
	}

	/*
	 * Валидные записи есть на обеих страницах.
	 */
	if(sequenceIsNewer(page_b_seq, page_a_seq))
	{
		store->last_addr = page_b_addr;
		store->last_seq = page_b_seq;
		store->active_page = cfg->page_b;
	}
	else
	{
		store->last_addr = page_a_addr;
		store->last_seq = page_a_seq;
		store->active_page = cfg->page_a;
	}

	return NV_OK;
}


NV_Status_t NV_Load(
	NVStore_t *store,
	void *out_payload,
	size_t len)
{
	if(store == NULL || out_payload == NULL)
	{
		return NV_BAD_PARAM;
	}

	NV_CFG_t *cfg = &store->cfg;

	if(len != (size_t)cfg->payload_len)
	{
		return NV_BAD_PARAM;
	}

	if(store->last_addr == 0u)
	{
		return NV_EMPTY;
	}

	uint32_t payload_addr =
		recordPayloadAddress(
			store->last_addr
		);

	memcpy(
		out_payload,
		(const void *)(uintptr_t)payload_addr,
		cfg->payload_len
	);

	return NV_OK;
}


NV_Status_t NV_Save(
	NVStore_t *store,
	const void *payload,
	size_t len)
{
	if(store == NULL || payload == NULL)
	{
		return NV_BAD_PARAM;
	}

	NV_CFG_t *cfg = &store->cfg;

	if(len != (size_t)cfg->payload_len)
	{
		return NV_BAD_PARAM;
	}

	if(store->active_page != cfg->page_a &&
	   store->active_page != cfg->page_b)
	{
		return NV_BAD_PARAM;
	}

	uint32_t target_page =
		store->active_page;

	uint32_t write_addr = 0u;

	NV_Status_t status =
		findEmptySlot(
			store,
			target_page,
			&write_addr
		);

	if(status == NV_BAD_PARAM)
	{
		return status;
	}

	if(status != NV_OK &&
	   status != NV_FULL)
	{
		return status;
	}

	if(flash_ll_unlock() != 0)
	{
		return NV_IO_FAIL;
	}

	/*
	 * Активная страница заполнена.
	 *
	 * Стираем вторую страницу, но старую пока
	 * не трогаем. Поэтому при сбое питания старая
	 * валидная запись сохраняется.
	 */
	if(status == NV_FULL)
	{
		target_page =
			otherPage(
				store,
				store->active_page
			);

		if(flash_ll_erase_page(
			target_page) != 0)
		{
			(void)flash_ll_lock();
			return NV_IO_FAIL;
		}

		status =
			findEmptySlot(
				store,
				target_page,
				&write_addr
			);

		if(status != NV_OK)
		{
			(void)flash_ll_lock();
			return NV_IO_FAIL;
		}
	}

	NV_HDR_t header;

	header.magic = cfg->magic;
	header.seq = store->last_seq + 1u;
	header.version = cfg->version;
	header.payload_len = cfg->payload_len;
	header.reserved = UINT32_MAX;

	NV_TAIL_t tail;

	tail.crc32 =
		recordCRC(
			&header,
			payload,
			cfg->payload_len
		);

	tail.commit = NV_COMMIT_MAGIC;

	uint32_t payload_addr =
		recordPayloadAddress(write_addr);

	uint32_t tail_addr =
		recordTailAddress(
			write_addr,
			cfg->payload_len
		);

	/*
	 * 1. Заголовок.
	 */
	status =
		programBytes(
			write_addr,
			&header,
			(uint32_t)sizeof(header)
		);

	if(status != NV_OK)
	{
		(void)flash_ll_lock();
		return status;
	}

	/*
	 * 2. Payload.
	 *
	 * programBytes() дополняет последний неполный
	 * doubleword байтами 0xFF.
	 */
	status =
		programBytes(
			payload_addr,
			payload,
			cfg->payload_len
		);

	if(status != NV_OK)
	{
		(void)flash_ll_lock();
		return status;
	}

	/*
	 * 3. CRC + commit записываются последними
	 * одним 64-битным doubleword.
	 *
	 * До выполнения этой операции запись
	 * считается невалидной.
	 */
	uint64_t tail_doubleword = UINT64_MAX;

	memcpy(
		&tail_doubleword,
		&tail,
		sizeof(tail)
	);

	if(flash_ll_program_doubleword(
		tail_addr,
		tail_doubleword) != 0)
	{
		(void)flash_ll_lock();
		return NV_IO_FAIL;
	}

	if(flash_ll_lock() != 0)
	{
		return NV_IO_FAIL;
	}

	/*
	 * Состояние в RAM меняем только после того,
	 * как commit успешно записан и Flash закрыта.
	 */
	store->last_addr = write_addr;
	store->last_seq = header.seq;
	store->active_page = target_page;

	return NV_OK;
}


uint32_t NV_LastSeq(
	const NVStore_t *store)
{
	if(store == NULL)
	{
		return 0u;
	}

	return store->last_seq;
}


#endif
