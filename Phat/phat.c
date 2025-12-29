#include "phat.h"

#include <string.h>

#pragma pack(push, 1)
typedef struct Phat_MBR_Entry_s
{
	uint8_t boot_indicator;
	uint8_t starting_head;
	uint8_t starting_sector;
	uint8_t starting_cylinder;
	uint8_t partition_type;
	uint8_t ending_head;
	uint8_t ending_sector;
	uint8_t ending_cylinder;
	uint32_t starting_LBA;
	uint32_t size_in_sectors;
}Phat_MBR_Entry_t, *Phat_MBR_Entry_p;

typedef struct Phat_MBR_s
{
	uint8_t boot_code[446];
	Phat_MBR_Entry_t partition_entries[4];
	uint16_t boot_signature;
}Phat_MBR_t, *Phat_MBR_p;

typedef struct Phat_DBR_s
{
	uint8_t jump_boot[3];
	uint8_t OEM_name[8];
	uint16_t bytes_per_sector;
	uint8_t sectors_per_cluster;
	uint16_t reserved_sector_count;
	uint8_t num_FATs;
	uint16_t root_entry_count;
	uint16_t total_sectors_16;
	uint8_t media;
	uint16_t FAT_size_16;
	uint16_t sectors_per_track;
	uint16_t num_heads;
	uint32_t hidden_sectors;
	uint32_t total_sectors_32;
	uint32_t FAT_size_32;
	uint16_t FATs_are_different;
	uint16_t version;
	uint32_t root_dir_cluster;
	uint16_t FS_info_sector;
	uint16_t backup_boot_sector;
	uint8_t reserved[12];
	uint16_t BIOS_drive_number;
	uint8_t extension_flag;
	uint32_t volume_ID;
	uint8_t volume_label[11];
	uint8_t file_system_type[8];
	uint8_t boot_code[420];
	uint16_t boot_sector_signature;
}Phat_DBR_t, *Phat_DBR_p;
#pragma pack(pop)

static PhatBool_t Phat_IsCachedSectorSync(Phat_SectorCache_p cached_sector)
{
	return (cached_sector->usage & SECTORCACHE_SYNC) == SECTORCACHE_SYNC;
}

static void Phat_SetCachedSectorUnsync(Phat_SectorCache_p cached_sector)
{
	cached_sector->usage &= ~SECTORCACHE_SYNC;
}

static void Phat_SetCachedSectorSync(Phat_SectorCache_p cached_sector)
{
	cached_sector->usage |= SECTORCACHE_SYNC;
}

static uint32_t Phat_GetCachedSectorAge(Phat_SectorCache_p cached_sector)
{
	return cached_sector->usage & SECTORCACHE_AGE_BM;
}

static void Phat_SetCachedSectorAge(Phat_SectorCache_p cached_sector, uint32_t age)
{
	cached_sector->usage &= ~SECTORCACHE_AGE_BM;
	cached_sector->usage |= (age & SECTORCACHE_AGE_BM);
}

static PhatBool_t Phat_IsCachedSectorValid(Phat_SectorCache_p cached_sector)
{
	return (cached_sector->usage & SECTORCACHE_VALID) == SECTORCACHE_VALID;
}

static void Phat_SetCachedSectorValid(Phat_SectorCache_p cached_sector)
{
	cached_sector->usage |= SECTORCACHE_VALID;
}

static PhatState Phat_WriteBackCachedSector(Phat_p phat, Phat_SectorCache_p cached_sector)
{
	if (!Phat_IsCachedSectorSync(cached_sector))
	{
		if (!phat->driver.fn_write_sector(cached_sector->data, cached_sector->LBA, 1, phat->driver.userdata))
		{
			return PhatState_WriteFail;
		}
		cached_sector->usage |= SECTORCACHE_SYNC;
	}
	return PhatState_OK;
}

static PhatState Phat_InvalidateCachedSector(Phat_p phat, Phat_SectorCache_p cached_sector)
{
	if (Phat_IsCachedSectorValid(cached_sector) && !Phat_IsCachedSectorSync(cached_sector))
	{
		if (!Phat_WriteBackCachedSector(phat, cached_sector))
		{
			return PhatState_WriteFail;
		}
		Phat_SetCachedSectorSync(cached_sector);
	}
	cached_sector->usage &= ~SECTORCACHE_VALID;
	return PhatState_OK;
}

static PhatState Phat_ReadSectorThroughCache(Phat_p phat, LBA_t LBA, Phat_SectorCache_p *pp_cached_sector)
{
	PhatState ret = PhatState_OK;
	for (size_t i = 0; i < PHAT_CACHED_SECTORS; i++)
	{
		Phat_SectorCache_p cached_sector = &phat->cache[i];
		if (Phat_IsCachedSectorValid(cached_sector) && phat->cache[i].LBA == LBA)
		{
			Phat_SetCachedSectorUnsync(cached_sector);
			*pp_cached_sector = cached_sector;
			return PhatState_OK;
		}
	}

	for (size_t i = 0; i < PHAT_CACHED_SECTORS; i++)
	{
		Phat_SectorCache_p cached_sector = &phat->cache[i];
		int MustDo = (i == PHAT_CACHED_SECTORS - 1);
		if (MustDo || !Phat_IsCachedSectorValid(cached_sector) || Phat_GetCachedSectorAge(cached_sector) - phat->LRU_age >= PHAT_CACHED_SECTORS)
		{
			ret = Phat_InvalidateCachedSector(phat, cached_sector);
			if (ret != PhatState_OK) return ret;
			if (!phat->driver.fn_read_sector(cached_sector->data, LBA, 1, phat->driver.userdata))
			{
				return PhatState_ReadFail;
			}
			phat->LRU_age++;
			cached_sector->LBA = LBA;
			Phat_SetCachedSectorValid(cached_sector);
			Phat_SetCachedSectorSync(cached_sector);
			Phat_SetCachedSectorAge(cached_sector, phat->LRU_age);
			*pp_cached_sector = cached_sector;
			return PhatState_OK;
		}
	}

	return PhatState_InternalError;
}

PhatState Phat_Init(Phat_p phat)
{
	memset(phat, 0, sizeof * phat);
	phat->driver = Phat_InitDriver(NULL);
	if (!Phat_OpenDevice(&phat->driver)) return PhatState_DriverError;
	return PhatState_OK;
}

}

PhatState Phat_DeInit(Phat_p phat)
{
	PhatState ret = PhatState_OK;
	for (size_t i = 0; i < PHAT_CACHED_SECTORS; i++)
	{
		Phat_SectorCache_p cached_sector = &phat->cache[i];
		if (Phat_IsCachedSectorValid(cached_sector))
		{
			ret = Phat_InvalidateCachedSector(phat, cached_sector);
			if (ret != PhatState_OK) return ret;
		}
	}
	if (!Phat_CloseDevice(&phat->driver)) return PhatState_DriverError;
	Phat_DeInitDriver(&phat->driver);
	memset(phat, 0, sizeof * phat);
	return PhatState_OK;
}

