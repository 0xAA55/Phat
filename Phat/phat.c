#include "phat.h"

#include <ctype.h>
#include <string.h>

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

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

typedef struct Phat_FSInfo_s
{
	uint32_t lead_signature;
	uint8_t reserved1[480];
	uint32_t struct_signature;
	uint32_t free_cluster_count;
	uint32_t next_free_cluster;
	uint8_t reserved2[14];
	uint16_t trail_signature;
}Phat_FSInfo_t, *Phat_FSInfo_p;

typedef struct Phat_DirItem_s
{
	uint8_t file_name_8_3[11];
	uint8_t attributes;
	uint8_t case_info;
	uint8_t creation_time_tenths;
	uint16_t creation_time;
	uint16_t creation_date;
	uint16_t last_access_date;
	uint16_t first_cluster_high;
	uint16_t last_modification_time;
	uint16_t last_modification_date;
	uint16_t first_cluster_low;
	uint32_t file_size;
}Phat_DirItem_t, *Phat_DirItem_p;

typedef struct Phat_LFN_Entry_s
{
	uint8_t order;
	WChar_t name1[5];
	uint8_t attributes; // Must be ATTRIB_LFN
	uint8_t type; // Must be 0
	uint8_t checksum;
	WChar_t name2[6];
	uint16_t first_cluster_low; // Must be 0
	WChar_t name3[2];
}Phat_LFN_Entry_t, *Phat_LFN_Entry_p;
#pragma pack(pop)

#define ATTRIB_LFN (ATTRIB_READ_ONLY | ATTRIB_HIDDEN | ATTRIB_SYSTEM | ATTRIB_VOLUME_ID)
#define CI_EXTENSION_IS_LOWER 0x08
#define CI_BASENAME_IS_LOWER 0x10

static PhatState Phat_ReadFAT(Phat_p phat, uint32_t cluster_index, uint32_t *read_out);
static PhatState Phat_WriteFAT(Phat_p phat, uint32_t cluster_index, uint32_t write);

static WChar_t Cp437_UpperPart[] =
{
	0x00c7, 0x00fc, 0x00e9, 0x00e2, 0x00e4, 0x00e0, 0x00e5, 0x00e7,
	0x00ea, 0x00eb, 0x00e8, 0x00ef, 0x00ee, 0x00ec, 0x00c4, 0x00c5,
	0x00c9, 0x00e6, 0x00c6, 0x00f4, 0x00f6, 0x00f2, 0x00fb, 0x00f9,
	0x00ff, 0x00d6, 0x00dc, 0x00a2, 0x00a3, 0x00a5, 0x20a7, 0x0192,
	0x00e1, 0x00ed, 0x00f3, 0x00fa, 0x00f1, 0x00d1, 0x00aa, 0x00ba,
	0x00bf, 0x2310, 0x00ac, 0x00bd, 0x00bc, 0x00a1, 0x00ab, 0x00bb,
	0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
	0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 0x255c, 0x255b, 0x2510,
	0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 0x255e, 0x255f,
	0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2567,
	0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b,
	0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580,
	0x03b1, 0x00df, 0x0393, 0x03c0, 0x03a3, 0x03c3, 0x00b5, 0x03c4,
	0x03a6, 0x0398, 0x03a9, 0x03b4, 0x221e, 0x03c6, 0x03b5, 0x2229,
	0x2261, 0x00b1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00f7, 0x2248,
	0x00b0, 0x2219, 0x00b7, 0x221a, 0x207f, 0x00b2, 0x25a0, 0x00a0,
};

static WChar_t Cp437_To_Unicode(uint8_t cp437_char)
{
	if (cp437_char < 0x80)
		return (WChar_t)cp437_char;
	else
		return Cp437_UpperPart[cp437_char - 0x80];
}

static void Phat_MoveCachedSectorHead(Phat_p phat, Phat_SectorCache_p sector)
{
	Phat_SectorCache_p prev = sector->prev;
	Phat_SectorCache_p next = sector->next;
	if (sector == phat->cache_LRU_head) return;
	if (prev) prev->next = next;
	if (next) next->prev = prev;
	if(sector == phat->cache_LRU_tail)
	{
		phat->cache_LRU_tail = prev;
		if (prev) prev->next = NULL;
	}
	if (phat->cache_LRU_head)
		phat->cache_LRU_head->prev = sector;
	sector->prev = NULL;
	sector->next = phat->cache_LRU_head;
	phat->cache_LRU_head = sector;
}

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

// Will write back if dirty
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
	Phat_SectorCache_p cache = phat->cache_LRU_head;

	if (!cache)
	{
		Phat_SectorCache_p ret_sector = &phat->cache[0];
		phat->cache_LRU_head = ret_sector;
		phat->cache_LRU_tail = &phat->cache[PHAT_CACHED_SECTORS - 1];
		for (size_t i = 0; i < PHAT_CACHED_SECTORS; i++)
		{
			Phat_SectorCache_p cached_sector = &phat->cache[i];
			cached_sector->prev = (i == 0) ? NULL : &phat->cache[i - 1];
			cached_sector->next = (i == PHAT_CACHED_SECTORS - 1) ? NULL : &phat->cache[i + 1];
			cached_sector->usage = 0;
		}
		if (!phat->driver.fn_read_sector(ret_sector->data, LBA, 1, phat->driver.userdata))
		{
			return PhatState_ReadFail;
		}
		ret_sector->LBA = LBA;
		Phat_SetCachedSectorValid(ret_sector);
		Phat_SetCachedSectorSync(ret_sector);
		*pp_cached_sector = ret_sector;
		return PhatState_OK;
	}

	while (cache)
	{
		if (cache->LBA == LBA && Phat_IsCachedSectorValid(cache))
		{
			*pp_cached_sector = cache;
			Phat_MoveCachedSectorHead(phat, cache);
			return PhatState_OK;
		}
		cache = cache->next;
	}

	cache = phat->cache_LRU_tail;
	ret = Phat_InvalidateCachedSector(phat, cache);
	if (ret != PhatState_OK) return ret;
	if (!phat->driver.fn_read_sector(cache->data, LBA, 1, phat->driver.userdata))
	{
		return PhatState_ReadFail;
	}
	cache->LBA = LBA;
	Phat_SetCachedSectorValid(cache);
	Phat_SetCachedSectorSync(cache);
	*pp_cached_sector = cache;
	Phat_MoveCachedSectorHead(phat, cache);
	return PhatState_OK;
}

static void Phat_SetCachedSectorModified(Phat_SectorCache_p p_cached_sector)
{
	Phat_SetCachedSectorUnsync(p_cached_sector);
}

// Will load the sector into cache if not present
static PhatState Phat_WriteSectorThroughCache(Phat_p phat, LBA_t LBA, const void *buffer)
{
	Phat_SectorCache_p cached_sector;
	PhatState ret = Phat_ReadSectorThroughCache(phat, LBA, &cached_sector);
	if (ret != PhatState_OK) return ret;
	memcpy(cached_sector->data, buffer, 512);
	Phat_SetCachedSectorModified(cached_sector);
	return PhatState_OK;
}

// Will also update cache if present
static PhatState Phat_ReadSectorsWithoutCache(Phat_p phat, LBA_t LBA, size_t num_sectors, void *buffer)
{
	if (!phat->driver.fn_read_sector(buffer, LBA, num_sectors, phat->driver.userdata))
	{
		return PhatState_ReadFail;
	}
	for (size_t i = 0; i < PHAT_CACHED_SECTORS; i++)
	{
		Phat_SectorCache_p cached_sector = &phat->cache[i];
		if (Phat_IsCachedSectorValid(cached_sector) && cached_sector->LBA >= LBA && cached_sector->LBA < LBA + num_sectors)
		{
			uint8_t *copy_from = (uint8_t *)buffer + (cached_sector->LBA - LBA) * 512;
			memcpy(cached_sector->data, copy_from, 512);
			Phat_SetCachedSectorSync(cached_sector);
		}
	}
	return PhatState_ReadFail;
}

// Will also update cache if present
static PhatState Phat_WriteSectorsWithoutCache(Phat_p phat, LBA_t LBA, size_t num_sectors, const void *buffer)
{
	if (!phat->driver.fn_write_sector(buffer, LBA, num_sectors, phat->driver.userdata))
	{
		return PhatState_WriteFail;
	}
	for (size_t i = 0; i < PHAT_CACHED_SECTORS; i++)
	{
		Phat_SectorCache_p cached_sector = &phat->cache[i];
		if (Phat_IsCachedSectorValid(cached_sector) && cached_sector->LBA >= LBA && cached_sector->LBA < LBA + num_sectors)
		{
			uint8_t *copy_from = (uint8_t *)buffer + (cached_sector->LBA - LBA) * 512;
			memcpy(cached_sector->data, copy_from, 512);
			Phat_SetCachedSectorSync(cached_sector);
		}
	}
	return PhatState_WriteFail;
}

static LBA_t Phat_CHS_to_LBA(uint8_t head, uint8_t sector, uint8_t cylinder)
{
	uint8_t actual_sector = sector & 0x1F;
	uint16_t actual_cylinder = ((uint16_t)(sector & 0xC0) << 2) | cylinder;
	if (actual_sector < 1) return 0;
	return ((LBA_t)actual_cylinder * 255 + head) * 63 + (actual_sector - 1);
}

static PhatBool_t Phat_GetMBREntryInfo(Phat_MBR_Entry_p entry, LBA_t *p_starting_LBA, LBA_t *p_size_in_sectors)
{
	LBA_t starting_LBA_from_CHS = Phat_CHS_to_LBA(entry->starting_head, entry->starting_sector, entry->starting_cylinder);
	LBA_t ending_LBA_from_CHS = Phat_CHS_to_LBA(entry->ending_head, entry->ending_sector, entry->ending_cylinder);
	LBA_t size_in_sectors_from_CHS = ending_LBA_from_CHS - starting_LBA_from_CHS + 1;
	if (entry->starting_LBA != 0 && entry->size_in_sectors != 0)
	{
		*p_starting_LBA = entry->starting_LBA;
		*p_size_in_sectors = entry->size_in_sectors;
	}
	else
	{
		if (ending_LBA_from_CHS < starting_LBA_from_CHS) return 0;
		*p_starting_LBA = starting_LBA_from_CHS;
		*p_size_in_sectors = size_in_sectors_from_CHS;
	}
	return 1;
}

static PhatBool_t Phat_IsSectorDBR(const Phat_DBR_p dbr)
{
	if (dbr->boot_sector_signature != 0xAA55) return 0;
	if (dbr->bytes_per_sector == 0) return 0;
	if (dbr->sectors_per_cluster == 0) return 0;
	if (dbr->reserved_sector_count == 0) return 0;
	if (dbr->num_FATs == 0) return 0;
	if (dbr->jump_boot[0] != 0xEB && dbr->jump_boot[0] != 0xE9) return 0;
	return 1;
}

static PhatBool_t Phat_IsSectorMBR(const Phat_MBR_p mbr)
{
	if (Phat_IsSectorDBR((Phat_DBR_p)mbr)) return 0;
	if (mbr->boot_signature != 0xAA55) return 0;
	for (size_t i = 0; i < 4; i++)
	{
		Phat_MBR_Entry_p e = &mbr->partition_entries[i];
		if (e->boot_indicator != 0 && e->boot_indicator != 0x80) return 0;
	}
	return 1;
}

PhatState Phat_Init(Phat_p phat)
{
	Phat_Date_t default_date =
	{
		PHAT_DEFAULT_YEAR,
		PHAT_DEFAULT_MONTH,
		PHAT_DEFAULT_DAY,
	};
	Phat_Time_t default_time =
	{
		PHAT_DEFAULT_HOUR,
		PHAT_DEFAULT_MINUTE,
		PHAT_DEFAULT_SECOND,
		0,
	};
	memset(phat, 0, sizeof * phat);
	phat->driver = Phat_InitDriver(NULL);
	if (!Phat_OpenDevice(&phat->driver)) return PhatState_DriverError;
	Phat_SetCurDateTime(phat, &default_date, &default_time);
	return PhatState_OK;
}

// Iterate through FAT to find a free cluster
static PhatState Phat_SearchForFreeCluster(Phat_p phat, uint32_t from_index, uint32_t *cluster_out)
{
	PhatState ret;
	uint32_t cluster;
	for (uint32_t i = from_index; i < phat->num_FAT_entries; i++)
	{
		ret = Phat_ReadFAT(phat, i, &cluster);
		if (ret != PhatState_OK) return ret;
		if (!cluster)
		{
			*cluster_out = i + 2;
			return PhatState_OK;
		}
	}
	return PhatState_NotEnoughSpace;
}

// Iterate through FAT to count free clusters
static PhatState Phat_SumFreeClusters(Phat_p phat, uint32_t *num_free_clusters_out)
{
	PhatState ret;
	uint32_t cluster;
	uint32_t sum = 0;
	for (uint32_t i = 0; i < phat->num_FAT_entries; i++)
	{
		ret = Phat_ReadFAT(phat, i, &cluster);
		if (ret != PhatState_OK) return ret;
		if (!cluster) sum++;
	}
	*num_free_clusters_out = sum;
	return PhatState_OK;
}

static LBA_t Phat_ClusterToLBA(Phat_p phat, uint32_t cluster)
{
	return phat->data_start_LBA + (LBA_t)(cluster - 2) * phat->sectors_per_cluster;
}

// Find next free cluster starting from phat->next_free_cluster
static PhatState Phat_SeekForFreeCluster(Phat_p phat, uint32_t *cluster_out)
{
	return Phat_SearchForFreeCluster(phat, phat->next_free_cluster - 2, cluster_out);
}

// Open a partition, load all of the informations from the DBR in order to manipulate files/directories
PhatState Phat_Mount(Phat_p phat, int partition_index)
{
	LBA_t partition_start_LBA = 0;
	LBA_t total_sectors = 0;
	LBA_t end_of_FAT_LBA;
	PhatState ret = PhatState_OK;
	Phat_SectorCache_p cached_sector;
	Phat_MBR_p mbr;
	Phat_DBR_p dbr;
	Phat_FSInfo_p fsi;
	ret = Phat_ReadSectorThroughCache(phat, partition_start_LBA, &cached_sector);
	if (ret != PhatState_OK) return ret;

	mbr = (Phat_MBR_p)cached_sector->data;

	// Incase of there could be a MBR, read partition info and get to the partition's DBR
	if (Phat_IsSectorMBR(mbr))
	{
		if (partition_index < 0 || partition_index >= 4) return PhatState_InvalidParameter;
		if (!Phat_GetMBREntryInfo(&mbr->partition_entries[partition_index], &partition_start_LBA, &total_sectors)) return PhatState_PartitionTableError;
		ret = Phat_ReadSectorThroughCache(phat, partition_start_LBA, &cached_sector);
		if (ret != PhatState_OK) return ret;
	}
	else if (partition_index != 0)
	{
		return PhatState_InvalidParameter;
	}

	dbr = (Phat_DBR_p)cached_sector->data;
	if (!Phat_IsSectorDBR(dbr)) return PhatState_FSNotFat;
	if (!memcmp(dbr->file_system_type, "FAT12   ", 8))
		phat->FAT_bits = 12;
	else if (!memcmp(dbr->file_system_type, "FAT16   ", 8))
		phat->FAT_bits = 16;
	else if (!memcmp(dbr->file_system_type, "FAT32   ", 8))
		phat->FAT_bits = 32;
	else
		return PhatState_FSNotFat;
	phat->FAT_size_in_sectors = (phat->FAT_bits == 32) ? dbr->FAT_size_32 : dbr->FAT_size_16;
	end_of_FAT_LBA = dbr->reserved_sector_count + (LBA_t)dbr->num_FATs * phat->FAT_size_in_sectors;
	phat->partition_start_LBA = partition_start_LBA;
	phat->total_sectors = total_sectors;
	phat->num_FATs = dbr->num_FATs;
	phat->FAT1_start_LBA = dbr->reserved_sector_count;
	phat->root_dir_cluster = (phat->FAT_bits == 32) ? dbr->root_dir_cluster : 0;
	phat->root_dir_start_LBA = end_of_FAT_LBA + ((phat->FAT_bits == 32) ? (LBA_t)(phat->root_dir_cluster - 2) * dbr->sectors_per_cluster : 0);
	phat->data_start_LBA = phat->root_dir_start_LBA + ((phat->FAT_bits == 32) ? 0 : (LBA_t)((dbr->root_entry_count * 32) + (dbr->bytes_per_sector - 1)) / dbr->bytes_per_sector);
	phat->bytes_per_sector = dbr->bytes_per_sector;
	phat->sectors_per_cluster = dbr->sectors_per_cluster;
	phat->num_diritems_in_a_sector = phat->bytes_per_sector / 32;
	phat->num_diritems_in_a_cluster = (phat->bytes_per_sector * phat->sectors_per_cluster) / 32;
	phat->num_FAT_entries = (phat->FAT_size_in_sectors * phat->bytes_per_sector * 8) / phat->FAT_bits;
	phat->FATs_are_same = !dbr->FATs_are_different;
	switch (phat->FAT_bits)
	{
	case 12: phat->end_of_cluster_chain = 0x0FF8; break;
	case 16: phat->end_of_cluster_chain = 0xFFF8; break;
	case 32: phat->end_of_cluster_chain = 0x0FFFFFF8; break;
	}
	phat->max_valid_cluster = phat->num_FAT_entries + 1;

	// Read FSInfo sector
	ret = Phat_ReadSectorThroughCache(phat, partition_start_LBA + 1, &cached_sector);
	if (ret != PhatState_OK) return ret;
	fsi = (Phat_FSInfo_p)cached_sector->data;
	if (fsi->lead_signature == 0x41615252 && fsi->struct_signature == 0x61417272 && fsi->trail_signature == 0xAA55)
	{
		phat->has_FSInfo = 1;
		phat->free_clusters = fsi->free_cluster_count;
		phat->next_free_cluster = fsi->next_free_cluster;
	}
	else
	{
		phat->has_FSInfo = 0;
		ret = Phat_SearchForFreeCluster(phat, 0, &phat->next_free_cluster);
		if (ret != PhatState_OK) phat->next_free_cluster = 2;
		ret = Phat_SumFreeClusters(phat, &phat->free_clusters);
		if (ret != PhatState_OK) phat->free_clusters = 0;
	}

	return PhatState_OK;
}

// Should be called after some allocations or deletions happened on the FAT
static PhatState Phat_UpdateFSInfo(Phat_p phat)
{
	PhatState ret = PhatState_OK;
	Phat_SectorCache_p cached_sector;
	Phat_FSInfo_p fsi;

	ret = Phat_ReadSectorThroughCache(phat, phat->partition_start_LBA + 1, &cached_sector);
	if (ret != PhatState_OK) return ret;

	fsi = (Phat_FSInfo_p)cached_sector->data;
	fsi->next_free_cluster = phat->next_free_cluster;
	fsi->free_cluster_count = phat->free_clusters;
	Phat_SetCachedSectorModified(cached_sector);
	return PhatState_OK;
}

PhatState Phat_FlushCache(Phat_p phat)
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
	return ret;
}

PhatState Phat_Unmount(Phat_p phat)
{
	return Phat_FlushCache(phat);
}

PhatState Phat_DeInit(Phat_p phat)
{
	PhatState ret = Phat_FlushCache(phat);
	if (ret != PhatState_OK) return ret;
	if (!Phat_CloseDevice(&phat->driver)) return PhatState_DriverError;
	Phat_DeInitDriver(&phat->driver);
	memset(phat, 0, sizeof * phat);
	return PhatState_OK;
}

void Phat_SetCurDateTime(Phat_p phat, Phat_Date_p cur_date, Phat_Time_p cur_time)
{
	phat->cur_date = *cur_date;
	phat->cur_time = *cur_time;
}

static PhatState Phat_WipeCluster(Phat_p phat, uint32_t cluster)
{
	PhatState ret;
	static const uint8_t empty_sector[512] = { 0 };
	LBA_t cluster_LBA = Phat_ClusterToLBA(phat, cluster) + phat->partition_start_LBA;
	for (LBA_t i = 0; i < phat->sectors_per_cluster; i++)
	{
		LBA_t LBA = cluster_LBA + i;
		PhatBool_t wiped = 0;
		ret = Phat_WriteSectorsWithoutCache(phat, LBA, 1, empty_sector);
		if (ret != PhatState_OK) return ret;
	}
	return PhatState_OK;
}

// Read FAT table by `cluster_index` starting from 0
static PhatState Phat_ReadFAT(Phat_p phat, uint32_t cluster_index, uint32_t *read_out)
{
	PhatState ret = PhatState_OK;
	uint16_t raw_entry;
	int half_cluster = 0;
	uint32_t fat_offset;
	LBA_t fat_sector_LBA;
	Phat_SectorCache_p cached_sector;
	uint32_t cluster_number;
	size_t ent_offset_in_sector;

	switch (phat->FAT_bits)
	{
	case 12:
		half_cluster = cluster_index & 1;
		fat_offset = cluster_index + (cluster_index >> 1);
		break;
	case 16:
		fat_offset = cluster_index * 2;
		break;
	case 32:
		fat_offset = cluster_index * 4;
		break;
	default:
		return PhatState_InternalError;
	}
	fat_sector_LBA = phat->FAT1_start_LBA + (fat_offset / phat->bytes_per_sector);
	ret = Phat_ReadSectorThroughCache(phat, fat_sector_LBA, &cached_sector);
	if (ret != PhatState_OK) return ret;
	ent_offset_in_sector = fat_offset % phat->bytes_per_sector;
	switch (phat->FAT_bits)
	{
	case 12:
		raw_entry = *(uint16_t *)&cached_sector->data[ent_offset_in_sector];
		if (half_cluster == 0)
			cluster_number = raw_entry & 0x0FFF;
		else
			cluster_number = (raw_entry >> 4) & 0x0FFF;
		break;
	case 16:
		cluster_number = *(uint16_t *)&cached_sector->data[ent_offset_in_sector];
		break;
	case 32:
		cluster_number = *(uint32_t *)&cached_sector->data[ent_offset_in_sector];
		break;
	}
	*read_out = cluster_number;
	return PhatState_OK;
}

// Write FAT table by `cluster_index` starting from 0
static PhatState Phat_WriteFAT(Phat_p phat, uint32_t cluster_index, uint32_t write)
{
	PhatState ret = PhatState_OK;
	int half_cluster = 0;
	uint32_t fat_offset;
	LBA_t fat_sector_LBA;
	Phat_SectorCache_p cached_sector;
	size_t ent_offset_in_sector;

	switch (phat->FAT_bits)
	{
	case 12:
		half_cluster = cluster_index & 1;
		fat_offset = cluster_index + (cluster_index >> 1);
		break;
	case 16:
		fat_offset = cluster_index * 2;
		break;
	case 32:
		fat_offset = cluster_index * 4;
		break;
	default:
		return PhatState_InternalError;
	}
	for (LBA_t i = 0; i < phat->num_FATs; i++)
	{
		fat_sector_LBA = phat->FAT1_start_LBA + i * phat->FAT_size_in_sectors + (fat_offset / phat->bytes_per_sector);
		ret = Phat_ReadSectorThroughCache(phat, fat_sector_LBA, &cached_sector);
		if (ret != PhatState_OK) return ret;
		ent_offset_in_sector = fat_offset % phat->bytes_per_sector;
		switch (phat->FAT_bits)
		{
		case 12:
		{
			uint16_t raw_entry = *(uint16_t *)&cached_sector->data[ent_offset_in_sector];
			if (write == 0x0FFFFFFF) write = 0x0FFF;
			if (half_cluster == 0)
			{
				raw_entry &= 0xF000;
				raw_entry |= (write & 0x0FFF);
			}
			else
			{
				raw_entry &= 0x000F;
				raw_entry |= (write & 0x0FFF) << 4;
			}
			*(uint16_t *)&cached_sector->data[ent_offset_in_sector] = raw_entry;
		}
		break;
		case 16:
			if (write == 0x0FFFFFFF) write = 0xFFFF;
			*(uint16_t *)&cached_sector->data[ent_offset_in_sector] = write;
			break;
		case 32:
			*(uint32_t *)&cached_sector->data[ent_offset_in_sector] = write;
			break;
		}
		Phat_SetCachedSectorModified(cached_sector);
		if (!phat->FATs_are_same) break;
	}
	return PhatState_OK;
}

static PhatState Phat_UnlinkCluster(Phat_p phat, uint32_t cluster_index)
{
	PhatState ret;
	uint32_t next_sector;
	uint32_t end_of_chain = phat->end_of_cluster_chain;
	for (;;)
	{
		if (cluster_index + 2 < phat->next_free_cluster) phat->next_free_cluster = cluster_index + 2;
		ret = Phat_ReadFAT(phat, cluster_index, &next_sector);
		if (ret != PhatState_OK) return ret;
		ret = Phat_WriteFAT(phat, cluster_index, 0);
		if (ret != PhatState_OK) return ret;
		if (next_sector >= end_of_chain) break;
		if (next_sector < 2 || next_sector > phat->max_valid_cluster) return PhatState_FATError;
		cluster_index = next_sector - 2;
	}
	return PhatState_OK;
}

static PhatState Phat_AllocateCluster(Phat_p phat, uint32_t *allocated_cluster)
{
	PhatState ret;
	uint32_t free_cluster;
	ret = Phat_SeekForFreeCluster(phat, &free_cluster);
	if (ret != PhatState_OK) return ret;
	ret = Phat_WriteFAT(phat, free_cluster, phat->end_of_cluster_chain);
	if (ret != PhatState_OK) return ret;
	*allocated_cluster = free_cluster;
	if (phat->has_FSInfo)
	{
		if (free_cluster == phat->next_free_cluster)
		{
			ret = Phat_SearchForFreeCluster(phat, free_cluster - 2 + 1, &phat->next_free_cluster);
			if (ret != PhatState_OK)
			{
				phat->next_free_cluster = 2;
			}
		}
		if (phat->free_clusters > 0)
			phat->free_clusters--;
		ret = Phat_UpdateFSInfo(phat);
		if (ret != PhatState_OK) return ret;
	}
	return PhatState_OK;
}

// The `cur_cluster` is not an index, it's a cluster number.
static PhatState Phat_GetFATNextCluster(Phat_p phat, uint32_t cur_cluster, uint32_t *next_cluster)
{
	PhatState ret = PhatState_OK;
	uint32_t cluster_number;
	if (cur_cluster < 2) return PhatState_InvalidParameter;
	if (cur_cluster > phat->max_valid_cluster) return PhatState_InvalidParameter;
	ret = Phat_ReadFAT(phat, cur_cluster - 2, &cluster_number);
	if (ret != PhatState_OK) return ret;
	if (cluster_number >= phat->end_of_cluster_chain) return PhatState_EndOfFATChain;
	if (cluster_number > phat->max_valid_cluster) return PhatState_FATError;
	if (cluster_number < 2) return PhatState_FATError;
	*next_cluster = cluster_number;
	return PhatState_OK;
}

static Phat_Time_t Phat_ParseTime(uint16_t time, uint8_t tenths)
{
	Phat_Time_t t =
	{
		(time >> 11) & 0x1F,
		(time >> 5) & 0x3F,
		(time & 0x1F) * 2,
		(tenths & 0x3F) * 10,
	};
	return t;
}

static Phat_Date_t Phat_ParseDate(uint16_t date)
{
	Phat_Date_t d =
	{
		((date >> 9) & 0x7F) + 1980,
		(date >> 5) & 0x0F,
		date & 0x1F,
	};
	return d;
}

static uint16_t Phat_EncodeTime(Phat_Time_p time)
{
	uint16_t raw_time = 0;
	if (time->hours > 23) time->hours = 23;
	if (time->minutes > 59) time->minutes = 59;
	if (time->seconds > 59) time->seconds = 59;
	raw_time |= (time->hours & 0x1F) << 11;
	raw_time |= (time->minutes & 0x3F) << 5;
	raw_time |= (time->seconds / 2) & 0x1F;
	return raw_time;
}

static uint16_t Phat_EncodeDate(Phat_Date_p date)
{
	uint16_t raw_date = 0;
	if (date->year < 1980) date->year = 1980;
	if (date->year > 2107) date->year = 2107;
	if (date->month < 1) date->month = 1;
	if (date->month > 12) date->month = 12;
	if (date->day < 1) date->day = 1;
	if (date->day > 31) date->day = 31;
	raw_date |= ((date->year - 1980) & 0x7F) << 9;
	raw_date |= (date->month & 0x0F) << 5;
	raw_date |= date->day & 0x1F;
	return raw_date;
}

static uint8_t Phat_LFN_ChkSum(uint8_t *file_name_8_3)
{
	uint8_t sum = 0;
	for (int i = 0; i < 11; i++) sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + *file_name_8_3++;
	return sum;
}

static PhatState Phat_UpdateClusterByDirItemIndex(Phat_DirInfo_p dir_info)
{
	PhatState ret = PhatState_OK;
	Phat_p phat = dir_info->phat;
	uint32_t cluster_index;
	uint32_t next_cluster;
	if (dir_info->dir_start_cluster < 2) return PhatState_InvalidParameter;
	cluster_index = dir_info->cur_diritem / phat->sectors_per_cluster;
	if (dir_info->dir_current_cluster_index > cluster_index)
	{
		dir_info->dir_current_cluster_index = 0;
		dir_info->dir_current_cluster = dir_info->dir_start_cluster;
	}
	while (dir_info->dir_current_cluster_index < cluster_index)
	{
		if (dir_info->dir_current_cluster < 2) return PhatState_InternalError;
		ret = Phat_GetFATNextCluster(phat, dir_info->dir_current_cluster, &next_cluster);
		if (ret == PhatState_EndOfFATChain)
			return PhatState_EndOfDirectory;
		else if (ret != PhatState_OK)
			return ret;
		if (next_cluster > phat->max_valid_cluster) return PhatState_FATError;
		dir_info->dir_current_cluster_index++;
		dir_info->dir_current_cluster = next_cluster;
	}
	return PhatState_OK;
}

static PhatState Phat_GetDirItem(Phat_DirInfo_p dir_info, Phat_DirItem_p dir_item)
{
	PhatState ret = PhatState_OK;
	LBA_t dir_sector_LBA;
	uint32_t item_index_in_sector;
	Phat_SectorCache_p cached_sector;
	Phat_DirItem_p dir_items;
	Phat_p phat = dir_info->phat;
	uint32_t cur_diritem_in_cur_cluster = dir_info->cur_diritem % phat->num_diritems_in_a_cluster;

	if (dir_info->dir_start_cluster == 0) return PhatState_EndOfDirectory;
	ret = Phat_UpdateClusterByDirItemIndex(dir_info);
	if (ret != PhatState_OK) return ret;
	if (phat->FAT_bits == 32)
		dir_sector_LBA = Phat_ClusterToLBA(phat, dir_info->dir_current_cluster) + cur_diritem_in_cur_cluster / phat->num_diritems_in_a_sector;
	else // Root directory in FAT12/16
		dir_sector_LBA = phat->root_dir_start_LBA + cur_diritem_in_cur_cluster / phat->num_diritems_in_a_sector;
	item_index_in_sector = cur_diritem_in_cur_cluster % phat->num_diritems_in_a_sector;
	dir_sector_LBA += phat->partition_start_LBA;
	ret = Phat_ReadSectorThroughCache(phat, dir_sector_LBA, &cached_sector);
	if (ret != PhatState_OK) return ret;
	dir_items = (Phat_DirItem_p)&cached_sector->data[0];
	*dir_item = dir_items[item_index_in_sector];
	return PhatState_OK;
}

static PhatState Phat_PutDirItem(Phat_DirInfo_p dir_info, const Phat_DirItem_p dir_item)
{
	PhatState ret = PhatState_OK;
	LBA_t dir_sector_LBA;
	uint32_t item_index_in_sector;
	Phat_SectorCache_p cached_sector;
	Phat_DirItem_p dir_items;
	Phat_p phat = dir_info->phat;
	uint32_t cur_diritem_in_cur_cluster = dir_info->cur_diritem % phat->num_diritems_in_a_cluster;

	if (dir_info->dir_start_cluster == 0)
	{
		uint32_t new_cluster;
		ret = Phat_AllocateCluster(phat, &new_cluster);
		if (ret != PhatState_OK) return ret;
		dir_info->dir_start_cluster = new_cluster;
		dir_info->dir_current_cluster = new_cluster;
		ret = Phat_WipeCluster(phat, new_cluster);
		if (ret != PhatState_OK) return ret;
	}
	ret = Phat_UpdateClusterByDirItemIndex(dir_info);
	if (ret != PhatState_OK) return ret;
	if (phat->FAT_bits == 32)
		dir_sector_LBA = Phat_ClusterToLBA(phat, dir_info->dir_current_cluster) + cur_diritem_in_cur_cluster / phat->num_diritems_in_a_sector;
	else // Root directory in FAT12/16
		dir_sector_LBA = phat->root_dir_start_LBA + cur_diritem_in_cur_cluster / phat->num_diritems_in_a_sector;
	item_index_in_sector = cur_diritem_in_cur_cluster % phat->num_diritems_in_a_sector;
	dir_sector_LBA += phat->partition_start_LBA;
	ret = Phat_ReadSectorThroughCache(phat, dir_sector_LBA, &cached_sector);
	if (ret != PhatState_OK) return ret;
	dir_items = (Phat_DirItem_p)&cached_sector->data[0];
	dir_items[item_index_in_sector] = *dir_item;
	Phat_SetCachedSectorModified(cached_sector);
	return PhatState_OK;
}

static PhatState Phat_SuckLFNIntoBuffer(Phat_LFN_Entry_p lfn_item, Phat_DirInfo_p buffer)
{
	uint8_t order = lfn_item->order & 0x3F;
	uint16_t write_pos = (order - 1) * 13;
	if (order < 1) return PhatState_FSError;
	if (write_pos > MAX_LFN) return PhatState_FSError;
	for (size_t i = 0; i < 5; i++)
	{
		WChar_t wchar = lfn_item->name1[i];
		if (write_pos > MAX_LFN) goto Ended;
		buffer->LFN_name[write_pos++] = wchar;
		if (!wchar) goto Ended;
	}
	for (size_t i = 0; i < 6; i++)
	{
		WChar_t wchar = lfn_item->name2[i];
		if (write_pos > MAX_LFN) goto Ended;
		buffer->LFN_name[write_pos++] = wchar;
		if (!wchar) goto Ended;
	}
	for (size_t i = 0; i < 2; i++)
	{
		WChar_t wchar = lfn_item->name3[i];
		if (write_pos > MAX_LFN) goto Ended;
		buffer->LFN_name[write_pos++] = wchar;
		if (!wchar) goto Ended;
	}
Ended:
	if (lfn_item->order & 0x40) buffer->LFN_length = write_pos - 1;
	return PhatState_OK;
}

static PhatBool_t Phat_IsValidLFNEntry(Phat_DirItem_p lfn_item)
{
	Phat_LFN_Entry_p lfne = (Phat_LFN_Entry_p)lfn_item;
	if (lfne->attributes != ATTRIB_LFN) return 0;
	if (lfne->type != 0) return 0;
	if (lfne->first_cluster_low != 0) return 0;
	return 1;
}

static PhatState Phat_MoveToNextDirItem(Phat_DirInfo_p dir_info)
{
	dir_info->cur_diritem++;
	return Phat_UpdateClusterByDirItemIndex(dir_info);
}

static PhatState Phat_MoveToNextDirItemWithAllocation(Phat_DirInfo_p dir_info)
{
	PhatState ret = PhatState_OK;
	uint32_t next_cluster;
	Phat_p phat = dir_info->phat;
	uint32_t cur_diritem_in_cur_cluster;

	// The directory item must have it's first allocated cluster, otherwise we cannot allocate more clusters to it
	if (dir_info->dir_start_cluster < 2) return PhatState_InternalError;

	// Sanity check
	if (dir_info->dir_start_cluster > phat->max_valid_cluster) return PhatState_FSError;

	dir_info->cur_diritem++;
	cur_diritem_in_cur_cluster = dir_info->cur_diritem % phat->num_diritems_in_a_cluster;
	if (cur_diritem_in_cur_cluster == 0)
	{
		dir_info->dir_current_cluster_index++;
		// Get to next cluster
		ret = Phat_GetFATNextCluster(phat, dir_info->dir_current_cluster, &next_cluster);
		if (ret == PhatState_OK)
		{
			dir_info->dir_current_cluster = next_cluster;
		}
		else if (ret == PhatState_EndOfFATChain)
		{
			// Allocate a new cluster
			ret = Phat_AllocateCluster(phat, &next_cluster);
			if (ret != PhatState_OK) return ret;
			ret = Phat_WriteFAT(phat, dir_info->dir_current_cluster - 2, next_cluster);
			if (ret != PhatState_OK) return ret;
			dir_info->dir_current_cluster = next_cluster;
			ret = Phat_WipeCluster(phat, next_cluster);
			if (ret != PhatState_OK) return ret;
			return PhatState_OK;
		}
		else
		{
			return ret;
		}
	}
	return ret;
}

PhatState Phat_NextDirItem(Phat_DirInfo_p dir_info)
{
	PhatState ret = PhatState_OK;
	Phat_DirItem_t diritem;
	Phat_LFN_Entry_p lfnitem;
	PhatBool_t no_checksum = 1;
	uint8_t checksum;
	Phat_p phat = dir_info->phat;

	dir_info->LFN_length = 0;
	for (;;)
	{
		ret = Phat_GetDirItem(dir_info, &diritem);
		if (ret != PhatState_OK) return ret;
		if (Phat_IsValidLFNEntry(&diritem))
		{
			// LFN entry
			lfnitem = (Phat_LFN_Entry_p)&diritem;
			if (no_checksum)
			{
				no_checksum = 0;
				dir_info->sfn_checksum = lfnitem->checksum;
			}
			else if (lfnitem->checksum != dir_info->sfn_checksum)
			{
				dir_info->sfn_checksum = lfnitem->checksum;
				dir_info->LFN_length = 0;
			}
			ret = Phat_SuckLFNIntoBuffer(lfnitem, dir_info);
			if (ret != PhatState_OK) return ret;
			ret = Phat_MoveToNextDirItem(dir_info);
			if (ret != PhatState_OK) return ret;
		}
		else if (diritem.file_name_8_3[0] == 0xE5)
		{
			// Deleted entry
			dir_info->LFN_length = 0;
			no_checksum = 1;
			ret = Phat_MoveToNextDirItem(dir_info);
			if (ret != PhatState_OK) return ret;
		}
		else if (diritem.file_name_8_3[0] == 0x00)
		{
			// End of directory
			dir_info->LFN_length = 0;
			return PhatState_EndOfDirectory;
		}
		else
		{
			// Standard entry
			checksum = Phat_LFN_ChkSum(diritem.file_name_8_3);
			if (!no_checksum && checksum != dir_info->sfn_checksum)
			{
				dir_info->LFN_length = 0;
			}
			memcpy(dir_info->file_name_8_3, diritem.file_name_8_3, 11);
			dir_info->attributes = diritem.attributes;
			dir_info->ctime = Phat_ParseTime(diritem.creation_time, diritem.creation_time_tenths);
			dir_info->cdate = Phat_ParseDate(diritem.creation_date);
			dir_info->mtime = Phat_ParseTime(diritem.last_modification_time, 0);
			dir_info->mdate = Phat_ParseDate(diritem.last_modification_date);
			dir_info->adate = Phat_ParseDate(diritem.last_access_date);
			dir_info->file_size = diritem.file_size;
			dir_info->first_cluster = ((uint32_t)diritem.first_cluster_high << 16) | diritem.first_cluster_low;
			if (dir_info->LFN_length == 0)
			{
				// No valid LFN, generate from 8.3 name
				size_t copy_to = 0;
				for (size_t i = 0; i < 8; i++)
				{
					uint8_t ch = diritem.file_name_8_3[i];
					if (!ch) break;
					if (ch != ' ') copy_to = i;
				}
				for (size_t i = 0; i <= copy_to; i++)
				{
					uint8_t ch = diritem.file_name_8_3[i];
					if (diritem.case_info & CI_BASENAME_IS_LOWER)
						ch = tolower(ch);
					dir_info->LFN_name[dir_info->LFN_length++] = Cp437_To_Unicode(ch);
				}
				if (diritem.file_name_8_3[8] != ' ')
				{
					dir_info->LFN_name[dir_info->LFN_length++] = L'.';
					for (size_t i = 8; i < 11; i++)
					{
						uint8_t ch = diritem.file_name_8_3[i];
						if (!ch) break;
						if (ch != ' ') copy_to = i;
					}
				}
				for (size_t i = 8; i <= copy_to; i++)
				{
					uint8_t ch = diritem.file_name_8_3[i];
					if (diritem.case_info & CI_EXTENSION_IS_LOWER)
						ch = tolower(ch);
					dir_info->LFN_name[dir_info->LFN_length++] = Cp437_To_Unicode(ch);
				}
			}
			dir_info->LFN_name[dir_info->LFN_length] = L'\0';
			Phat_MoveToNextDirItem(dir_info);
			return PhatState_OK;
		}
	}
}

void Phat_CloseDir(Phat_DirInfo_p dir_info)
{
	memset(dir_info, 0, sizeof * dir_info);
}

void Phat_ToUpperDirectoryPath(WChar_p path)
{
	size_t length = 0;
	PhatBool_t tail_trimming = 1;
	for (length = 0; path[length]; length++) {}
	while (length > 0)
	{
		length--;
		if (path[length] == '/' || path[length] == '\\')
		{
			if (!tail_trimming)
				return;
		}
		else
			tail_trimming = 0;
		path[length] = L'\0';
	}
}

static WChar_p Phat_ToEndOfString(WChar_p string)
{
	while (*string) string++;
	return string;
}

void Phat_NormalizePath(WChar_p path)
{
	WChar_p read_ptr;
	WChar_p write_ptr;
	WChar_p start_ptr;
	size_t length;
	read_ptr = path;
	write_ptr = path;
	while (*read_ptr == '/' || *read_ptr == '\\') read_ptr++;
	start_ptr = read_ptr;
	for (;;)
	{
		if (*read_ptr == L'/' || *read_ptr == L'\\' || *read_ptr == L'\0')
		{
			length = (size_t)(read_ptr - start_ptr);
			if (length == 1 && start_ptr[0] == L'.')
			{
				start_ptr = read_ptr + 1;
				*write_ptr = L'\0';
			}
			else if (length == 2 && start_ptr[0] == L'.' && start_ptr[1] == L'.')
			{
				*start_ptr = L'\0';
				Phat_ToUpperDirectoryPath(path);
				write_ptr = Phat_ToEndOfString(path);
				start_ptr = read_ptr + 1;
			}
			else if (length)
			{
				while (start_ptr < read_ptr)
				{
					*write_ptr++ = *start_ptr++;
				}
				*write_ptr++ = L'/';
				start_ptr = read_ptr + 1;
			}
			else
			{
				start_ptr = read_ptr + 1;
			}
			if (*read_ptr == L'\0') break;
		}
		read_ptr++;
	}
	*write_ptr = L'\0';
	if (write_ptr > path && (*(write_ptr - 1) == L'/' || *(write_ptr - 1) == L'\\'))
	{
		*(write_ptr - 1) = L'\0';
	}
}

void Phat_PathToName(WChar_p path, WChar_p name)
{
	WChar_p chr = Phat_ToEndOfString(path);
	size_t length = 0;
	while (chr > path)
	{
		if (*chr == L'/' || *chr == L'\\')
			*chr-- = L'\0';
		else
			break;
	}
	while (chr > path)
	{
		if (*chr != L'/' && *chr != L'\\')
		{
			chr--;
			length++;
		}
		else
		{
			break;
		}
	}
	if (*chr == L'/' || *chr == L'\\')
	{
		chr++;
		length--;
	}
	if (chr > path)
	{
		for (size_t i = 0; i <= length; i++)
		{
			name[i] = chr[i];
		}
	}
	else
	{
		name[0] = L'\0';
	}
}

void Phat_PathToNameInPlace(WChar_p path)
{
	Phat_PathToName(path, path);
}

PhatState Phat_OpenDir(Phat_p phat, WChar_p path, Phat_DirInfo_p dir_info)
{
	LBA_t cur_dir_sector = phat->root_dir_start_LBA;
	uint32_t cur_dir_cluster = phat->root_dir_cluster;
	WChar_p ptr = path;
	WChar_p name_start;
	size_t name_len;
	PhatState ret = PhatState_OK;

	// After normalized, all of the slashes were replaced by '/', and the leading/trailing/duplicated slashes were removed
	Phat_NormalizePath(path);
	memset(dir_info, 0, sizeof * dir_info);
	dir_info->phat = phat;
	dir_info->first_cluster = cur_dir_cluster;

	for (;;)
	{
		name_start = ptr;
		while (*ptr != L'\0' && *ptr != L'/') ptr++;
		name_len = (size_t)(ptr - name_start);
		if (name_len > MAX_LFN) return PhatState_InvalidPath;

		if ((*ptr || name_start == path) && name_len) // is middle path
		{
			dir_info->dir_start_cluster = cur_dir_cluster;
			dir_info->dir_current_cluster = cur_dir_cluster;
			dir_info->dir_current_cluster_index = 0;
			dir_info->cur_diritem = 0;
			for (;;)
			{
				ret = Phat_NextDirItem(dir_info);
				if (ret == PhatState_EndOfDirectory)
				{
					return PhatState_DirectoryNotFound;
				}
				else if (ret != PhatState_OK)
				{
					return ret;
				}
				if (!memcmp(dir_info->LFN_name, name_start, name_len * sizeof(WChar_t)) && name_len == dir_info->LFN_length)
				{
					if ((dir_info->attributes & ATTRIB_DIRECTORY) == 0)
					{
						return PhatState_NotADirectory;
					}
					cur_dir_cluster = dir_info->first_cluster;
					break;
				}
			}
		}
		else // is last path
		{
			cur_dir_cluster = dir_info->first_cluster;
			dir_info->dir_start_cluster = cur_dir_cluster;
			dir_info->dir_current_cluster = cur_dir_cluster;
			dir_info->dir_current_cluster_index = 0;
			dir_info->cur_diritem = 0;
			return PhatState_OK;
		}
	}
}

// Open a dir to the path, find the item if can (`PhatState_OK` will be returned), or return `PhatState_EndOfDirectory`
static PhatState Phat_FindItem(Phat_p phat, WChar_p path, Phat_DirInfo_p dir_info)
{
	PhatState ret;
	WChar_p longname = phat->filename_buffer;
	size_t name_len;

	Phat_PathToName(path, longname);
	if (longname[0] == L'\0') return PhatState_InvalidParameter;
	Phat_ToUpperDirectoryPath(path);

	ret = Phat_OpenDir(phat, path, dir_info);
	if (ret != PhatState_OK) return ret;

	name_len = (size_t)(Phat_ToEndOfString(longname) - longname);

	for (;;)
	{
		ret = Phat_NextDirItem(dir_info);
		if (ret != PhatState_OK) return ret;
		if (dir_info->LFN_length == name_len && !memcmp(longname, dir_info->LFN_name, name_len * sizeof(WChar_t)))
		{
			return PhatState_OK;
		}
	}
}

static PhatState Phat_FindFile(Phat_p phat, WChar_p path, Phat_DirInfo_p dir_info)
{
	PhatState ret = Phat_FindItem(phat, path, dir_info);
	switch (ret)
	{
	case PhatState_OK:
		if (dir_info->attributes & ATTRIB_DIRECTORY)
			return PhatState_IsADirectory;
		return PhatState_OK;
	case PhatState_EndOfDirectory:
		return PhatState_FileNotFound;
	default:
		return ret;
	}
}

static PhatState Phat_FindDirectory(Phat_p phat, WChar_p path, Phat_DirInfo_p dir_info)
{
	PhatState ret = Phat_FindItem(phat, path, dir_info);
	switch (ret)
	{
	case PhatState_OK:
		if (dir_info->attributes & ATTRIB_DIRECTORY)
			return PhatState_OK;
		return PhatState_NotADirectory;
	case PhatState_EndOfDirectory:
		return PhatState_DirectoryNotFound;
	default:
		return ret;
	}
}

PhatBool_t Phat_IsValidFilename(WChar_p filename)
{
	size_t length = 0;
	while (filename[length]) length++;
	if (length == 0 || length > MAX_LFN) return 0;
	for (size_t i = 0; i < length; i++)
	{
		WChar_t ch = filename[i];
		if (ch == L'\"' || ch == L'*' || ch == L'/' || ch == L':' ||
			ch == L'<' || ch == L'>' || ch == L'?' || ch == L'\\' ||
			ch == L'|' || ch < 0x0020)
		{
			return 0;
		}
	}
	if (length == 1 && filename[0] == L'.') return 0;
	if (length == 2 && filename[0] == L'.' && filename[1] == L'.') return 0;
	return 1;
}

static PhatBool_t Phat_IsFit83(WChar_p filename, uint8_t *sfn83, uint8_t *case_info)
{
	PhatBool_t bn_has_lower = 0;
	PhatBool_t bn_has_upper = 0;
	PhatBool_t ext_has_lower = 0;
	PhatBool_t ext_has_upper = 0;
	int length = 0;
	int dot = -1;
	while (filename[length]) length++;
	if (length == 0 || length > 11) return 0;
	for (int i = 0; i < length; i++)
	{
		WChar_t ch = filename[i];
		if (ch == ' ') return 0;
		if (ch >= L'A' && ch <= L'Z')
		{
			if (i < 8) bn_has_upper = 1;
			else ext_has_upper = 1;
		}
		else if (ch >= L'a' && ch <= L'z')
		{
			if (i < 8) bn_has_lower = 1;
			else ext_has_lower = 1;
		}
		else if (ch == L'.')
		{
			dot = i;
			if (i > 8) return 0;
			if ((length - dot) > 4) return 0;
		}
		else if (ch >= 128) return 0;
		if (bn_has_upper && bn_has_lower) return 0;
		if (ext_has_upper && ext_has_lower) return 0;
	}
	if (dot < 0 && length > 8) return 0;
	memset(sfn83, ' ', 11);
	*case_info = 0;
	if (bn_has_lower) *case_info |= CI_BASENAME_IS_LOWER;
	if (ext_has_lower) *case_info |= CI_EXTENSION_IS_LOWER;
	if (dot < 0)
	{
		for(int i = 0; i < length; i++) sfn83[i] = toupper(filename[i]);
	}
	else
	{
		for(int i = 0; i < dot; i++) sfn83[i] = toupper(filename[i]);
		for(int i = dot + 1; i < length; i++) sfn83[8 + (i - (dot + 1))] = toupper(filename[i]);
	}
	return 1;
}

static PhatState Phat_FindShortFileName(Phat_p phat, WChar_p path, uint8_t *sfn83, PhatBool_p found)
{
	PhatState ret;
	Phat_DirInfo_t dir_info;

	*found = 0;
	ret = Phat_OpenDir(phat, path, &dir_info);
	if (ret != PhatState_OK) return ret;
	for (;;)
	{
		ret = Phat_NextDirItem(&dir_info);
		if (ret == PhatState_EndOfDirectory)
		{
			Phat_CloseDir(&dir_info);
			return PhatState_OK;
		}
		else if (ret != PhatState_OK)
		{
			Phat_CloseDir(&dir_info);
			return ret;
		}
		if (!memcmp(dir_info.file_name_8_3, sfn83, 11))
		{
			*found = 1;
			Phat_CloseDir(&dir_info);
			return PhatState_OK;
		}
	}
}

static PhatState Phat_Gen83NameForLongFilename(Phat_p phat, WChar_p path, uint8_t *sfn83)
{
	PhatBool_t found = 0;
	PhatState ret;
	WChar_p tail;
	WChar_p dot;
	WChar_p filename;

	// Find the basename in a path, it's not allowed to have trailing slash in the tail
	tail = Phat_ToEndOfString(path);
	while (--tail > path)
	{
		if (*tail == L'/' || *tail == L'\\') *tail = L'\0';
		else break;
	}
	filename = tail++;
	while (filename > path)
	{
		if (*filename == L'/' || *filename == L'\\')
		{
			filename++;
			break;
		}
		else
		{
			filename--;
		}
	}

	memset(sfn83, 0x20, 11);
	while (*filename == L'.') filename++;
	while (tail > filename)
	{
		tail--;
		if (*tail == L'.') *tail = L'\0';
	}
	dot = tail;
	while (dot > filename)
	{
		dot--;
		if (*dot == L'.')
		{
			dot++;
			break;
		}
	}
	if (dot == filename)
	{
		for (size_t i = 0; i < 8 && filename[i]; i++)
		{
			WChar_t ch = toupper(filename[i]);
			sfn83[i] = (uint8_t)ch;
		}
	}
	else
	{
		size_t end = dot - filename;
		if (end > 8) end = 8;
		for (size_t i = 0; i < end && filename[i]; i++)
		{
			WChar_t ch = toupper(filename[i]);
			sfn83[i] = (uint8_t)ch;
		}
		for (size_t i = 0; i < 3 && dot[i]; i++)
		{
			WChar_t ch = toupper(dot[i]);
			sfn83[8 + i] = (uint8_t)ch;
		}
	}
	if (sfn83[0] == ' ')
	{
		memcpy(sfn83, "NONAME", 6);
	}

	for (uint32_t index = 1;;)
	{
		ret = Phat_FindShortFileName(phat, path, sfn83, &found);
		if (ret != PhatState_OK) return ret;
		if (!found) return PhatState_OK;
		if (index < 10)
		{
			sfn83[6] = '~';
			sfn83[7] = '0' + (uint8_t)index;
			index++;
		}
		else if (index < 100)
		{
			sfn83[5] = '~';
			sfn83[6] = '0' + (uint8_t)(index / 10);
			sfn83[7] = '0' + (uint8_t)(index % 10);
			index++;
		}
		else if (index < 1000)
		{
			sfn83[4] = '~';
			sfn83[5] = '0' + (uint8_t)(index / 100);
			sfn83[6] = '0' + (uint8_t)((index / 10) % 10);
			sfn83[7] = '0' + (uint8_t)(index % 10);
			index++;
		}
		else if (index < 10000)
		{
			sfn83[3] = '~';
			sfn83[4] = '0' + (uint8_t)(index / 1000);
			sfn83[5] = '0' + (uint8_t)((index / 100) % 10);
			sfn83[6] = '0' + (uint8_t)((index / 10) % 10);
			sfn83[7] = '0' + (uint8_t)(index % 10);
			index++;
		}
		else if (index < 100000)
		{
			sfn83[2] = '~';
			sfn83[3] = '0' + (uint8_t)(index / 10000);
			sfn83[4] = '0' + (uint8_t)((index / 1000) % 10);
			sfn83[5] = '0' + (uint8_t)((index / 100) % 10);
			sfn83[6] = '0' + (uint8_t)((index / 10) % 10);
			sfn83[7] = '0' + (uint8_t)(index % 10);
			index++;
		}
		else if (index < 1000000)
		{
			sfn83[1] = '~';
			sfn83[2] = '0' + (uint8_t)(index / 100000);
			sfn83[3] = '0' + (uint8_t)((index / 10000) % 10);
			sfn83[4] = '0' + (uint8_t)((index / 1000) % 10);
			sfn83[5] = '0' + (uint8_t)((index / 100) % 10);
			sfn83[6] = '0' + (uint8_t)((index / 10) % 10);
			sfn83[7] = '0' + (uint8_t)(index % 10);
			index++;
		}
		else
		{
			return PhatState_FSError;
		}
	}
}

static PhatState Phat_CreateNewItemInDir(Phat_p phat, WChar_p path, uint8_t attrib)
{
	Phat_DirInfo_t dir_info;
	PhatState ret = PhatState_OK;
	WChar_p longname = phat->filename_buffer;
	uint8_t name83[11];
	uint8_t case_info = 0;
	int only83 = 0;
	uint32_t fnlen = 0;
	uint32_t items_needed;
	uint32_t first_diritem = 0;
	uint32_t free_count = 0;
	Phat_DirItem_t dir_item;
	WChar_p ptr;
	uint32_t first_cluster = 0;

	Phat_NormalizePath(path);
	ptr = Phat_ToEndOfString(path);
	if (ptr == path) return PhatState_InvalidParameter;

	Phat_PathToName(path, longname);
	if (!Phat_IsValidFilename(longname)) return PhatState_InvalidParameter;
	while (longname[fnlen]) fnlen++;

	// Check if file/directory already exists
	Phat_DirInfo_t dir_info_find;
	ret = Phat_FindItem(phat, path, &dir_info_find);
	switch (ret)
	{
	case PhatState_OK:
		if (attrib & ATTRIB_DIRECTORY)
			return PhatState_DirectoryAlreadyExists;
		else
			return PhatState_FileAlreadyExists;
	case PhatState_EndOfDirectory:
		break;
	default:
		return ret;
	}

	if (Phat_IsFit83(longname, name83, &case_info))
	{
		only83 = 1;
		items_needed = 1;
	}
	else
	{
		ret = Phat_Gen83NameForLongFilename(phat, path, name83);
		if (ret != PhatState_OK) return ret;
		items_needed = 1 + ((fnlen + 12) / 13);
	}

	Phat_ToUpperDirectoryPath(path);
	ret = Phat_OpenDir(phat, path, &dir_info);
	if (ret != PhatState_OK) return ret;
	if (dir_info.dir_start_cluster == 0)
	{
		// New directory without contents, allocate cluster for it
		uint32_t new_cluster;
		ret = Phat_AllocateCluster(phat, &new_cluster);
		if (ret != PhatState_OK) return ret;
		dir_info.dir_start_cluster = new_cluster;
		dir_info.dir_current_cluster = new_cluster;
		ret = Phat_WipeCluster(phat, new_cluster);
		if (ret != PhatState_OK) return ret;
	}
	for (;;)
	{
		free_count = 0;
		ret = Phat_GetDirItem(&dir_info, &dir_item);
		if (ret != PhatState_OK) return ret;
		if (dir_item.file_name_8_3[0] == 0x00 || dir_item.file_name_8_3[0] == 0xE5)
		{
			first_diritem = dir_info.cur_diritem;
			free_count = 1;
			ret = Phat_MoveToNextDirItemWithAllocation(&dir_info);
			if (ret != PhatState_OK) return ret;
			while (free_count < items_needed)
			{
				ret = Phat_GetDirItem(&dir_info, &dir_item);
				if (ret != PhatState_OK) return ret;
				if (dir_item.file_name_8_3[0] == 0x00 || dir_item.file_name_8_3[0] == 0xE5)
				{
					free_count++;
					if (free_count == items_needed)
						break;
					ret = Phat_MoveToNextDirItemWithAllocation(&dir_info);
					if (ret != PhatState_OK) return ret;
				}
				else
				{
					break;
				}
			}
			if (free_count == items_needed)
				break;
		}
		else
		{
			ret = Phat_MoveToNextDirItemWithAllocation(&dir_info);
			if (ret != PhatState_OK) return ret;
		}
		if (free_count == items_needed)
			break;
	}
	Phat_CloseDir(&dir_info);
	ret = Phat_OpenDir(phat, path, &dir_info);
	if (ret != PhatState_OK) return ret;
	dir_info.cur_diritem = first_diritem;
	ret = Phat_UpdateClusterByDirItemIndex(&dir_info);
	if (ret != PhatState_OK) return ret;
	if (attrib & ATTRIB_DIRECTORY)
	{
		Phat_SectorCache_p cached_sector = NULL;
		Phat_DirItem_p dir_items;
		ret = Phat_AllocateCluster(phat, &first_cluster);
		if (ret != PhatState_OK) return ret;
		ret = Phat_WipeCluster(phat, first_cluster);
		if (ret != PhatState_OK) return ret;
		ret = Phat_ReadSectorThroughCache(phat, Phat_ClusterToLBA(phat, first_cluster) + phat->partition_start_LBA, &cached_sector);
		if (ret != PhatState_OK) return ret;
		dir_items = (Phat_DirItem_p)cached_sector->data;
		memcpy(dir_items[0].file_name_8_3, ".          ", 11);
		dir_items[0].attributes = ATTRIB_DIRECTORY;
		dir_items[0].case_info = 0;
		dir_items[0].creation_time_tenths = 0;
		dir_items[0].creation_time = Phat_EncodeTime(&phat->cur_time);
		dir_items[0].creation_date = Phat_EncodeDate(&phat->cur_date);
		dir_items[0].last_access_date = Phat_EncodeDate(&phat->cur_date);
		dir_items[0].first_cluster_high = first_cluster >> 16;
		dir_items[0].last_modification_time = Phat_EncodeTime(&phat->cur_time);
		dir_items[0].last_modification_date = Phat_EncodeDate(&phat->cur_date);
		dir_items[0].first_cluster_low = first_cluster & 0xFFFF;
		dir_items[0].file_size = 0;
		memcpy(dir_items[1].file_name_8_3, "..         ", 11);
		dir_items[1].attributes = ATTRIB_DIRECTORY;
		dir_items[1].case_info = 0;
		dir_items[1].creation_time_tenths = 0;
		dir_items[1].creation_time = Phat_EncodeTime(&phat->cur_time);
		dir_items[1].creation_date = Phat_EncodeDate(&phat->cur_date);
		dir_items[1].last_access_date = Phat_EncodeDate(&phat->cur_date);
		dir_items[1].first_cluster_high = dir_info.dir_start_cluster >> 16;
		dir_items[1].last_modification_time = Phat_EncodeTime(&phat->cur_time);
		dir_items[1].last_modification_date = Phat_EncodeDate(&phat->cur_date);
		dir_items[1].first_cluster_low = dir_info.dir_start_cluster & 0xFFFF;
		dir_items[1].file_size = 0;
	}
	if (!only83)
	{
		uint8_t checksum = Phat_LFN_ChkSum(name83);
		uint8_t lfn_entries_needed = (uint8_t)(items_needed - 1);
		for (uint8_t i = lfn_entries_needed; i > 0; i--)
		{
			Phat_LFN_Entry_t lfn_entry;
			size_t copy_len = 13;
			size_t offset = (size_t)(i - 1) * 13;
			if (fnlen - offset < 13) copy_len = fnlen - offset;
			memset(&lfn_entry, 0, sizeof lfn_entry);
			lfn_entry.order = i;
			if (i == lfn_entries_needed) lfn_entry.order |= 0x40;
			lfn_entry.attributes = ATTRIB_LFN;
			lfn_entry.type = 0;
			lfn_entry.first_cluster_low = 0;
			lfn_entry.checksum = checksum;
			for (size_t j = 0; j < copy_len; j++)
			{
				WChar_t ch = longname[offset + j];
				if (j < 5)
					lfn_entry.name1[j] = ch;
				else if (j < 11)
					lfn_entry.name2[j - 5] = ch;
				else
					lfn_entry.name3[j - 11] = ch;
			}
			ret = Phat_PutDirItem(&dir_info, (Phat_DirItem_p)&lfn_entry);
			if (ret != PhatState_OK)
			{
				Phat_CloseDir(&dir_info);
				return ret;
			}
			Phat_MoveToNextDirItem(&dir_info);
		}
	}
	memset(&dir_item, 0, sizeof dir_item);
	memcpy(dir_item.file_name_8_3, name83, 11);
	dir_item.attributes = attrib;
	dir_item.creation_date = Phat_EncodeDate(&phat->cur_date);
	dir_item.creation_time = Phat_EncodeTime(&phat->cur_time);
	dir_item.creation_time_tenths = 0;
	dir_item.last_modification_date = Phat_EncodeDate(&phat->cur_date);
	dir_item.last_modification_time = Phat_EncodeTime(&phat->cur_time);
	dir_item.last_access_date = Phat_EncodeDate(&phat->cur_date);
	dir_item.first_cluster_low = first_cluster & 0xFFFF;
	dir_item.first_cluster_high = first_cluster >> 16;
	dir_item.file_size = 0;
	ret = Phat_PutDirItem(&dir_info, &dir_item);
	if (ret != PhatState_OK)
	{
		if (first_cluster > 2) Phat_UnlinkCluster(phat, first_cluster - 2);
		Phat_CloseDir(&dir_info);
		return ret;
	}
	Phat_CloseDir(&dir_info);
	return PhatState_OK;
}

PhatState Phat_OpenFile(Phat_p phat, WChar_p path, PhatBool_t readonly, Phat_FileInfo_p file_info)
{
	Phat_DirInfo_t dir_info;
	PhatState ret = PhatState_OK;

	ret = Phat_OpenDir(phat, path, &dir_info);
	if (ret != PhatState_OK) return ret;
	for (;;)
	{
		ret = Phat_NextDirItem(&dir_info);
		if (ret == PhatState_EndOfDirectory)
		{
			if (readonly)
			{
				Phat_CloseDir(&dir_info);
				return PhatState_FileNotFound;
			}
			else
			{
				Phat_CloseDir(&dir_info);
				ret = Phat_CreateNewItemInDir(phat, path, 0);
				if (ret != PhatState_OK) return ret;
			}
		}
		else if (ret != PhatState_OK)
		{
			Phat_CloseDir(&dir_info);
			return ret;
		}

		if (!memcmp(dir_info.LFN_name, Phat_ToEndOfString(path) - dir_info.LFN_length, dir_info.LFN_length) && dir_info.LFN_length == (size_t)(Phat_ToEndOfString(path) - path))
		{
			if (dir_info.attributes & ATTRIB_DIRECTORY)
			{
				return PhatState_IsADirectory;
			}
			file_info->phat = phat;
			file_info->first_cluster = dir_info.first_cluster;
			file_info->cur_cluster = dir_info.first_cluster;
			file_info->file_size = dir_info.file_size;
			file_info->readonly = readonly || ((dir_info.attributes & ATTRIB_READ_ONLY) != 0);
			file_info->file_pointer = 0;
			file_info->cur_cluster_index = 0;
			file_info->buffer_LBA = Phat_ClusterToLBA(phat, file_info->first_cluster) + phat->partition_start_LBA;
			if (file_info->file_size)
			{
				ret = Phat_ReadSectorsWithoutCache(phat, file_info->buffer_LBA, 1, file_info->sector_buffer);
				if (ret != PhatState_OK)
				{
					Phat_CloseDir(&dir_info);
					return ret;
				}
			}
			Phat_CloseDir(&dir_info);
			return PhatState_OK;
		}
	}

	return ret;
}

static PhatState Phat_UpdateClusterByFilePointer(Phat_FileInfo_p file_info)
{
	PhatState ret = PhatState_OK;
	Phat_p phat = file_info->phat;
	uint32_t cluster_index;
	uint32_t next_cluster;
	cluster_index = file_info->file_pointer / phat->sectors_per_cluster;
	if (file_info->cur_cluster_index > cluster_index)
	{
		file_info->cur_cluster_index = 0;
		file_info->cur_cluster = file_info->first_cluster;
	}
	while (file_info->cur_cluster_index < cluster_index)
	{
		ret = Phat_GetFATNextCluster(phat, file_info->cur_cluster, &next_cluster);
		if (ret == PhatState_EndOfFATChain)
			return PhatState_FATError;
		else if (ret != PhatState_OK)
			return ret;
		file_info->cur_cluster_index++;
		file_info->cur_cluster = next_cluster;
	}
	return PhatState_OK;
}

static PhatState Phat_GetCurFilePointerLBA(Phat_FileInfo_p file_info, LBA_p LBA_out)
{
	uint32_t offset_in_cluster;
	PhatState ret = Phat_UpdateClusterByFilePointer(file_info);
	if (ret != PhatState_OK) return ret;
	offset_in_cluster = (file_info->file_pointer / file_info->phat->bytes_per_sector) % file_info->phat->sectors_per_cluster;
	*LBA_out = Phat_ClusterToLBA(file_info->phat, file_info->cur_cluster) + offset_in_cluster + file_info->phat->partition_start_LBA;
	return PhatState_OK;
}

PhatState Phat_ReadFile(Phat_FileInfo_p file_info, void *buffer, uint32_t bytes_to_read, uint32_t *bytes_read)
{
	PhatState ret = PhatState_OK;
	Phat_p phat = file_info->phat;
	uint32_t offset_in_sector;
	LBA_t FPLBA;
	size_t sectors_to_read;

	*bytes_read = 0;
	if (file_info->first_cluster == 0) return PhatState_EndOfFile;
	if (file_info->file_pointer >= file_info->file_size) return PhatState_EndOfFile;
	if (file_info->file_pointer + bytes_to_read > file_info->file_size)
		bytes_to_read = file_info->file_size - file_info->file_pointer;
	offset_in_sector = file_info->file_pointer % 512;
	ret = Phat_GetCurFilePointerLBA(file_info, &FPLBA);
	if (ret != PhatState_OK) return ret;
	if (offset_in_sector)
	{
		size_t to_copy;
		if (file_info->buffer_LBA != FPLBA)
		{
			ret = Phat_ReadSectorsWithoutCache(phat, FPLBA, 1, file_info->sector_buffer);
			if (ret != PhatState_OK) return ret;
			file_info->buffer_LBA = FPLBA;
		}
		to_copy = 512 - offset_in_sector;
		if (to_copy > bytes_to_read) to_copy = bytes_to_read;
		memcpy(buffer, &file_info->sector_buffer[offset_in_sector], to_copy);
		buffer = (uint8_t *)buffer + to_copy;
		bytes_to_read -= (uint32_t)to_copy;
		file_info->file_pointer += (uint32_t)to_copy;
		*bytes_read += (uint32_t)to_copy;
	}
	sectors_to_read = bytes_to_read / 512;
	while (sectors_to_read)
	{
		sectors_to_read--;
		ret = Phat_GetCurFilePointerLBA(file_info, &FPLBA);
		if (ret != PhatState_OK) return ret;
		ret = Phat_ReadSectorsWithoutCache(phat, FPLBA, 1, buffer);
		if (ret != PhatState_OK) return ret;
		buffer = (uint8_t *)buffer + 512;
		file_info->file_pointer += 512;
		*bytes_read += 512;
		bytes_to_read -= 512;
	}
	if (bytes_to_read)
	{
		ret = Phat_GetCurFilePointerLBA(file_info, &FPLBA);
		if (ret != PhatState_OK) return ret;
		if (file_info->buffer_LBA != FPLBA)
		{
			ret = Phat_ReadSectorsWithoutCache(phat, FPLBA, 1, file_info->sector_buffer);
			if (ret != PhatState_OK) return ret;
			file_info->buffer_LBA = FPLBA;
		}
		ret = Phat_ReadSectorsWithoutCache(phat, FPLBA, 1, file_info->sector_buffer);
		if (ret != PhatState_OK) return ret;
		file_info->buffer_LBA = FPLBA;
		memcpy(buffer, file_info->sector_buffer, bytes_to_read);
		file_info->file_pointer += bytes_to_read;
		*bytes_read += 512;
	}
	return file_info->file_pointer >= file_info->file_size ? PhatState_EndOfFile : PhatState_OK;
}

PhatState Phat_SeekFile(Phat_FileInfo_p file_info, uint32_t position)
{
	PhatState ret;
	if (position > file_info->file_size)
	{
		position = file_info->file_size;
		file_info->file_pointer = position;
		ret = Phat_UpdateClusterByFilePointer(file_info);
		if (ret != PhatState_OK) return ret;
		return PhatState_EndOfFile;
	}
	file_info->file_pointer = position;
	return Phat_UpdateClusterByFilePointer(file_info);
}

void Phat_GetFilePointer(Phat_FileInfo_p file_info, uint32_t *position)
{
	*position = file_info->file_pointer;
}

void Phat_GetFileSize(Phat_FileInfo_p file_info, uint32_t *size)
{
	*size = file_info->file_size;
}

void Phat_CloseFile(Phat_FileInfo_p file_info)
{
	memset(file_info, 0, sizeof * file_info);
}

PhatState Phat_CreateDirectory(Phat_p phat, WChar_p path)
{
	return Phat_CreateNewItemInDir(phat, path, ATTRIB_DIRECTORY);
}

PhatState Phat_RemoveDirectory(Phat_p phat, WChar_p path)
{
	PhatState ret;
	Phat_DirInfo_t dir_info;
	size_t name_len;

	ret = Phat_OpenDir(phat, path, &dir_info);
	if (ret != PhatState_OK) return ret;
	ret = Phat_NextDirItem(&dir_info);
	if (ret == PhatState_OK) return PhatState_DirectoryNotEmpty;
	if (ret != PhatState_EndOfDirectory) return ret;
	Phat_CloseDir(&dir_info);

	Phat_PathToName(path, phat->filename_buffer);
	Phat_ToUpperDirectoryPath(path);
	name_len = (size_t)(Phat_ToEndOfString(phat->filename_buffer) - phat->filename_buffer);

	ret = Phat_OpenDir(phat, path, &dir_info);
	if (ret != PhatState_OK) return ret;

	for (;;)
	{
		uint32_t last_dir_item;
		uint32_t cur_dir_item;
		Phat_DirItem_t dir_item;
		last_dir_item = dir_info.cur_diritem;
		ret = Phat_NextDirItem(&dir_info);
		cur_dir_item = dir_info.cur_diritem;
		if (ret == PhatState_EndOfDirectory)
		{
			Phat_CloseDir(&dir_info);
			return PhatState_InternalError;
		}
		else if (ret != PhatState_OK)
		{
			Phat_CloseDir(&dir_info);
			return ret;
		}
		if (name_len == dir_info.LFN_length && !memcmp(phat->filename_buffer, dir_info.LFN_name, name_len * sizeof(WChar_t)))
		{
			dir_info.dir_current_cluster_index = 0;
			dir_info.dir_current_cluster = dir_info.dir_start_cluster;
			for (uint32_t i = last_dir_item; i <= cur_dir_item; i++)
			{
				dir_info.cur_diritem = i;
				ret = Phat_GetDirItem(&dir_info, &dir_item);
				if (ret != PhatState_OK) return ret;
				dir_item.file_name_8_3[0] = 0xE5;
				ret = Phat_PutDirItem(&dir_info, &dir_item);
				if (ret != PhatState_OK) return ret;
			}
			return PhatState_OK;
		}
	}
}
