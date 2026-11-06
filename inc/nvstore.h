#ifndef NVSTORE_H
#define NVSTORE_H
#include <stdint.h>
#include <stddef.h>


typedef enum{
	NV_OK = 0,
	NV_EMPTY,
	NV_CRC_FAIL,
	NV_IO_FAIL,
	NV_BAD_PARAM,
	NV_FULL = 7
}NV_Status_t;

typedef struct{
	uint32_t page_a;
	uint32_t page_b;
	uint32_t page_size;
	uint32_t magic;
	uint16_t version;
	uint32_t payload_len;
}NV_CFG_t;


typedef struct{
	NV_CFG_t cfg;
	uint32_t step;
	uint32_t last_addr;
	uint32_t last_seq;
	uint32_t active_page;
}NVStore_t;


NV_Status_t NV_Init(NVStore_t *store);
NV_Status_t NV_Load(NVStore_t *store, void *out_payload, size_t len);
NV_Status_t NV_Save(NVStore_t *store, const void* payload, size_t len);
uint32_t NV_LastSeq(const NVStore_t *store);
#endif
