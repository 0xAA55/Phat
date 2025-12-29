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

typedef uint16_t WChar_t, *WChar_p;

typedef struct Phat_SectorCache_s
{
	uint8_t data[512];
	LBA_t	LBA;
	uint32_t usage;
}Phat_SectorCache_t, *Phat_SectorCache_p;

typedef struct Phat_s
{
	Phat_SectorCache_t cache[PHAT_CACHED_SECTORS];
	Phat_Disk_Driver_t driver;
	uint32_t LRU_age;
	LBA_t partition_start_LBA;
	LBA_t total_sectors;
	LBA_t FAT_size_in_sectors;
	LBA_t FAT1_start_LBA;
	LBA_t root_dir_start_LBA;
	LBA_t data_start_LBA;
	uint8_t FAT_bits;
	uint8_t num_FATs;
	uint16_t bytes_per_sector;
	uint8_t sectors_per_cluster;
	uint8_t num_diritems_in_a_sector;
	uint16_t num_diritems_in_a_cluster;
}Phat_t, *Phat_p;

typedef enum PhatState_e
{
	PhatState_OK = 0,
	PhatState_InvalidParameter,
	PhatState_InternalError,
	PhatState_DriverError,
	PhatState_ReadFail,
	PhatState_WriteFail,
	PhatState_PartitionTableError,
	PhatState_PartitionError,
	PhatState_FATError,
	PhatState_DirectoryNotFound,
	PhatState_FileNotFound,
	PhatState_EndOfDirectory,
	PhatState_EndOfFATChain,
}PhatState;

PhatState Phat_Init(Phat_p phat);
PhatState Phat_DeInit(Phat_p phat);

PhatState Phat_Mount(Phat_p phat, int partition_index);

#endif
