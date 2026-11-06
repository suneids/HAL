#include "../../../inc/nvstore.h"
#if defined(STM32F103C6Tx)
#include <stdint.h>
#include <stddef.h>
#include "../inc/flash_ll.h"

#define NV_COMMIT_MAGIC 0xA55AC33Cu
#define NV_MAX_PAYLOAD  256u

typedef struct {
	uint32_t magic;
	uint32_t seq;
	uint16_t version;
	uint16_t payload_len;
	uint32_t reserved;
} NV_HDR_t;

typedef struct {
	uint32_t crc32;
	uint32_t commit;
} NV_TAIL_t;

_Static_assert(sizeof(NV_HDR_t) == 16u, "NV_HDR_t must be 16 bytes");
_Static_assert(sizeof(NV_TAIL_t) == 8u,  "NV_TAIL_t must be 8 bytes");

static uint32_t align8(uint32_t x){
	return (x + 7u) & ~7u;
}

static uint32_t recordStep(uint16_t payload_len)
{
	uint32_t tail_offset =
		align8((uint32_t)sizeof(NV_HDR_t) + payload_len);

	return tail_offset + (uint32_t)sizeof(NV_TAIL_t);
}

static uint32_t otherPage(NVStore_t *store, uint32_t p){
	NV_CFG_t *cfg = &store->cfg;
	return (p == cfg->page_a) ? cfg->page_b : cfg->page_a;
}


static uint32_t crc32_sw(const void *data, uint32_t len){
	uint32_t crc = 0xFFFFFFFFu;
	const uint8_t *p = (const uint8_t*)data;
	for(uint32_t i = 0; i < len; i++){
		crc ^= p[i];
		for(uint32_t b = 0; b < 8; b++){
			uint32_t mask = -(crc & 1u);
			crc = (crc >> 1) ^ (0xEDB88320u & mask);
		}
	}
	return ~crc;
}


static NV_Status_t scanPage(NVStore_t *store, uint32_t page_addr, uint32_t* out_best_addr, uint32_t* out_best_seq){
	NV_CFG_t *cfg = &store->cfg;
	*out_best_addr = 0;
	*out_best_seq = 0;

	uint32_t end = page_addr + cfg->page_size;

	for(uint32_t addr = page_addr; addr + sizeof(NV_HDR_t) <= end; addr += store->step){
		uint16_t state;
		if(flash_ll_read_halfword(addr, &state) != 0) return NV_IO_FAIL;

		if(state == NV_STATE_EMPTY) break;
		if(state == NV_STATE_WRITING) break;
		if(state != NV_STATE_VALID) continue;

		NV_HDR_t h = {0};
		uint16_t *hw = (uint16_t *)&h;
		for(uint32_t i = 0; i < sizeof(NV_HDR_t)/2; i++){
			if(flash_ll_read_halfword(addr + i * 2u, &hw[i]) != 0) return NV_IO_FAIL; // h's halfword read
		}

		if(h.magic != cfg->magic) continue;
		if(h.version != cfg->version) continue;
		if(h.payload_len != cfg->payload_len) continue;

		uint8_t payload[NV_MAX_PAYLOAD];
		uint32_t payload_addr = addr + (uint32_t)sizeof(NV_HDR_t);

		if(payload_addr + (uint32_t)cfg->payload_len + 4u > end) break;

		for(uint32_t i = 0; i < (uint32_t)cfg->payload_len; i += 2u){
			uint16_t w;
			if(flash_ll_read_halfword(payload_addr + i, &w) != 0) return NV_IO_FAIL;
			payload[i] = (uint8_t)(w & 0xFFu);
			if(i + 1u < (uint32_t)cfg->payload_len){
				payload[i + 1u] = (uint8_t)((w >> 8) & 0xFFu);
			}
		}

		uint32_t crc_addr = payload_addr + (uint32_t)cfg->payload_len;

		uint16_t c0, c1;
		if(flash_ll_read_halfword(crc_addr + 0u, &c0) != 0) return NV_IO_FAIL;
		if(flash_ll_read_halfword(crc_addr + 2u, &c1) != 0) return NV_IO_FAIL;

		uint32_t stored_crc = ((uint32_t)c1 << 16) | (uint32_t)c0;

		uint32_t calc_crc = crc32_sw(payload, (uint32_t)cfg->payload_len);

		if(calc_crc != stored_crc){
			continue;
		}

		if(h.seq >= *out_best_seq){
			*out_best_seq = h.seq;
			*out_best_addr = addr;
		}
	}
	return NV_OK;
}


static NV_Status_t findEmptySlot(const NVStore_t *store, uint32_t page_addr, uint32_t *out_addr){
	uint32_t end = page_addr + store->cfg.page_size;
	for(uint32_t addr = page_addr; addr + sizeof(NV_HDR_t) <= end; addr += store->step){
		uint16_t state;
		if(flash_ll_read_halfword(addr, &state) != 0) return NV_IO_FAIL;
		if(state == NV_STATE_EMPTY){
			*out_addr = addr;
			return NV_OK;
		}
	}
	return NV_FULL;
}


static NV_Status_t ProgramBytesAsHalfwords(uint32_t addr, const uint8_t *data, uint32_t len){
	for(uint32_t i = 0; i < len; i += 2u){
		uint16_t w = data[i];
		if(i + 1u < len) w |= (uint16_t)((uint16_t)data[i + 1u]) << 8;
		if(flash_ll_program_halfword(addr + i, w) != 0) return NV_IO_FAIL;
	}
	return NV_OK;
}



NV_Status_t NV_Init(NVStore_t *store){
	NV_CFG_t *cfg = &store->cfg;
	if(!store || !cfg) return NV_BAD_PARAM;
	if(cfg->page_size == 0u) return NV_BAD_PARAM;
	if(cfg->payload_len == 0u) return NV_BAD_PARAM;
	if(cfg->payload_len > NV_MAX_PAYLOAD) return NV_BAD_PARAM;
	if((cfg->page_a % cfg->page_size) != 0u) return NV_BAD_PARAM;
	if((cfg->page_b % cfg->page_size) != 0u) return NV_BAD_PARAM;
	if(cfg->page_a == cfg->page_b) return NV_BAD_PARAM;


	uint32_t rec_len = (uint32_t)sizeof(NV_HDR_t) + (uint32_t)cfg->payload_len + 4u;  // 4 - CRC32
	if(rec_len > cfg->page_size) return NV_BAD_PARAM;

	store->step = align2(rec_len);

	uint32_t a_addr, b_addr;
	uint32_t a_seq, b_seq;
	NV_Status_t st = scanPage(store, cfg->page_a, &a_addr, &a_seq);
	if(st != NV_OK) return st;
	st = scanPage(store, cfg->page_b, &b_addr, &b_seq);
	if(st != NV_OK) return st;
	if(a_addr == 0u && b_addr == 0u){
		if(flash_ll_unlock() != 0) return NV_IO_FAIL;
		if(flash_ll_erase_page(cfg->page_a) != 0){ flash_ll_lock(); return NV_IO_FAIL; }
		if(flash_ll_erase_page(cfg->page_b) != 0){ flash_ll_lock(); return NV_IO_FAIL; }
		if(flash_ll_lock() != 0) return NV_IO_FAIL;

		store->last_addr = 0;
		store->last_seq = 0;
		store->active_page = cfg->page_a;

		return NV_EMPTY;
	}
	if(b_seq >= a_seq) { store->last_addr = b_addr; store->last_seq = b_seq; }
	else			   { store->last_addr = a_addr; store->last_seq = a_seq; }
	if((store->last_addr >= cfg->page_a) && (store->last_addr < cfg->page_a + cfg->page_size)) store->active_page = cfg->page_a;
	else 																					   store->active_page = cfg->page_b;

	return NV_OK;
}


NV_Status_t NV_Load(NVStore_t *store, void *out_payload, size_t len){
	NV_CFG_t *cfg = &store->cfg;
	if(!out_payload) return NV_BAD_PARAM;
	if(len != (size_t)cfg->payload_len) return NV_BAD_PARAM;
	if(store->last_addr == 0u) return NV_EMPTY;
	uint8_t *dst = (uint8_t*)out_payload;
	uint32_t payload_addr = store->last_addr + (uint32_t)sizeof(NV_HDR_t);

	for(uint32_t i = 0; i < (uint32_t)cfg->payload_len; i += 2u){
		uint16_t w;
		if(flash_ll_read_halfword(payload_addr + i, &w) != 0) return NV_IO_FAIL;
		dst[i] = (uint8_t)(w & 0xFFu);

		if(i + 1u < (uint32_t)cfg->payload_len){
			dst[i + 1u] = (uint8_t)((w >> 8) & 0xFFu);
		}
	}
	return NV_OK;
}


NV_Status_t NV_Save(NVStore_t *store, const void* payload, size_t len){
	if(!payload) return NV_BAD_PARAM;
	NV_CFG_t *cfg = &store->cfg;
	if(len != (size_t)cfg->payload_len) return NV_BAD_PARAM;

	uint32_t wr_addr = 0;
	NV_Status_t st = findEmptySlot(store, store->active_page, &wr_addr);
	if(st == NV_IO_FAIL) return NV_IO_FAIL;

	if(flash_ll_unlock() != 0) return NV_IO_FAIL;

	if(st == NV_FULL){// Active page is full
		uint32_t new_page = otherPage(store, store->active_page);

		if(flash_ll_erase_page(new_page) != 0){
			flash_ll_lock();
			return NV_IO_FAIL;
		}

		st = findEmptySlot(store, new_page, &wr_addr);
		if(st != NV_OK){
			flash_ll_lock();
			return NV_IO_FAIL;
		}

		store->active_page = new_page;
	}


	NV_HDR_t h;
	h.state = NV_STATE_WRITING;
	h.version = cfg->version;
	h.magic = cfg->magic;
	h.seq = store->last_seq + 1u;
	h.payload_len = cfg->payload_len;
	h.reserved = 0u;

	uint32_t crc = crc32_sw(payload, (uint32_t)cfg->payload_len);
	{
		const uint16_t *hw = (const uint16_t *)&h;
		for(uint32_t i = 0; i < sizeof(NV_HDR_t) / 2u; i++){
			if(flash_ll_program_halfword(wr_addr + i * 2u, hw[i]) != 0){
				flash_ll_lock();
				return NV_IO_FAIL;
			}
		}
	}

	uint32_t payload_addr = wr_addr + (uint32_t)sizeof(NV_HDR_t);
	st = ProgramBytesAsHalfwords(payload_addr, (const uint8_t*)payload, (uint32_t)cfg->payload_len);
	if(st != NV_OK){
		flash_ll_lock();
		return st;
	}

	uint32_t crc_addr = payload_addr + (uint32_t)cfg->payload_len;
	uint16_t c0 = (uint16_t)(crc & 0xFFFFu);
	uint16_t c1 = (uint16_t)(crc >> 16);
	if(flash_ll_program_halfword(crc_addr + 0u, c0) != 0){ flash_ll_lock(); return NV_IO_FAIL; }
	if(flash_ll_program_halfword(crc_addr + 2u, c1) != 0){ flash_ll_lock(); return NV_IO_FAIL; }

	if(flash_ll_program_halfword(wr_addr, NV_STATE_VALID) != 0){
		flash_ll_lock();
		return NV_IO_FAIL;
	}

	if(flash_ll_lock() != 0) return NV_IO_FAIL;

	store->last_addr = wr_addr;
	store->last_seq = h.seq;
	return NV_OK;
}



uint32_t NV_LastSeq(const NVStore_t *store){
	return store->last_seq;
}
#endif
