#ifndef _PHAT_H_
#define _PHAT_H_ 1

#include <stdint.h>

#include "BSP_phat.h"

#ifndef PHAT_CACHED_SECTORS
#define PHAT_CACHED_SECTORS 8
#endif

#define SECTORCACHE_SYNC 0x80000000
#define SECTORCACHE_VALID 0x40000000
#define SECTORCACHE_AGE_BM 0x3FFFFFFF

typedef struct Phat_SectorCache_s
{
	uint8_t data[512];
	LBA_t	LBA;
	uint32_t usage;
}Phat_SectorCache_t, *Phat_SectorCache_p;

typedef struct Phat_s
{
	Phat_SectorCache_t cache[PHAT_CACHED_SECTORS];
	uint32_t LRU_age;
	Phat_Disk_Driver_t driver;
}Phat_t, *Phat_p;

PhatBool_t Phat_Init(Phat_p phat);
PhatBool_t Phat_DeInit(Phat_p phat);

#endif
