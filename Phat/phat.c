#include "phat.h"

#include <string.h>

#ifndef UNUSED(x)
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

typedef struct Phat_DirItem_s
{
	uint8_t file_name_8_3[11];
	uint8_t attributes;
	uint8_t reserved;
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

#define ATTRIB_READ_ONLY 0x01
#define ATTRIB_HIDDEN 0x02
#define ATTRIB_SYSTEM 0x04
#define ATTRIB_VOLUME_ID 0x08
#define ATTRIB_DIRECTORY 0x10
#define ATTRIB_ARCHIVE 0x20
#define ATTRIB_LFN (ATTRIB_READ_ONLY | ATTRIB_HIDDEN | ATTRIB_SYSTEM | ATTRIB_VOLUME_ID)

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

static void Phat_SetCachedSectorModified(Phat_SectorCache_p p_cached_sector)
{
	Phat_SetCachedSectorUnsync(p_cached_sector);
}

static PhatState Phat_ReadSectorsWithoutCache(Phat_p phat, LBA_t LBA, size_t num_sectors, void *buffer)
{
	if (phat->driver.fn_read_sector(buffer, LBA, num_sectors, phat->driver.userdata))
		return PhatState_OK;
	return PhatState_ReadFail;
}

static PhatState Phat_WriteSectorsWithoutCache(Phat_p phat, LBA_t LBA, size_t num_sectors, const void *buffer)
{
	if (phat->driver.fn_write_sector(buffer, LBA, num_sectors, phat->driver.userdata))
		return PhatState_OK;
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
	memset(phat, 0, sizeof * phat);
	phat->driver = Phat_InitDriver(NULL);
	if (!Phat_OpenDevice(&phat->driver)) return PhatState_DriverError;
	return PhatState_OK;
}

PhatState Phat_Mount(Phat_p phat, int partition_index)
{
	LBA_t partition_start_LBA = 0;
	LBA_t total_sectors = 0;
	PhatState ret = PhatState_OK;
	Phat_SectorCache_p cached_sector;
	Phat_MBR_p mbr;
	Phat_DBR_p dbr;
	ret = Phat_ReadSectorThroughCache(phat, 0, &cached_sector);
	if (ret != PhatState_OK) return ret;

	mbr = (Phat_MBR_p)cached_sector->data;
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
	if (!Phat_IsSectorDBR(dbr)) return PhatState_PartitionError;
	if (!memcmp(dbr->file_system_type, "FAT12   ", 8))
		phat->FAT_bits = 12;
	else if (!memcmp(dbr->file_system_type, "FAT16   ", 8))
		phat->FAT_bits = 16;
	else if (!memcmp(dbr->file_system_type, "FAT32   ", 8))
		phat->FAT_bits = 32;
	else
		return PhatState_PartitionError;
	phat->partition_start_LBA = partition_start_LBA;
	phat->total_sectors = total_sectors;
	phat->num_FATs = dbr->num_FATs;
	phat->FAT_size_in_sectors = (phat->FAT_bits == 32) ? dbr->FAT_size_32 : dbr->FAT_size_16;
	phat->FAT1_start_LBA = partition_start_LBA + dbr->reserved_sector_count;
	phat->root_dir_start_LBA = (phat->FAT_bits == 32) ? (LBA_t)dbr->root_dir_cluster * dbr->sectors_per_cluster : (LBA_t)phat->FAT1_start_LBA + (LBA_t)phat->FAT_size_in_sectors * phat->num_FATs;
	phat->data_start_LBA = phat->root_dir_start_LBA + ((phat->FAT_bits == 32) ? 0 : (LBA_t)((dbr->root_entry_count * 32) + (dbr->bytes_per_sector - 1)) / dbr->bytes_per_sector);
	phat->bytes_per_sector = dbr->bytes_per_sector;
	phat->sectors_per_cluster = dbr->sectors_per_cluster;
	phat->num_diritems_in_a_sector = phat->bytes_per_sector / 32;
	phat->num_diritems_in_a_cluster = (phat->bytes_per_sector * phat->sectors_per_cluster) / 32;

	return PhatState_OK;
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

static PhatState Phat_GetFATNextCluster(Phat_p phat, uint32_t cur_cluster, uint32_t *next_cluster)
{
	PhatState ret = PhatState_OK;
	uint32_t fat_offset;
	LBA_t fat_sector_LBA;
	int half_cluster = 0;
	uint16_t raw_entry;
	uint32_t cluster_number;
	if (cur_cluster < 2) return PhatState_FATError;
	cur_cluster -= 2;
	switch (phat->FAT_bits)
	{
	case 12:
		half_cluster = cur_cluster & 1;
		fat_offset = cur_cluster + (cur_cluster >> 1);
		break;
	case 16:
		fat_offset = cur_cluster * 2;
		break;
	case 32:
		fat_offset = cur_cluster * 4;
		break;
	default:
		return 0;
	}
	fat_sector_LBA = phat->FAT1_start_LBA + (fat_offset / phat->bytes_per_sector);
	Phat_SectorCache_p cached_sector;
	ret = Phat_ReadSectorThroughCache(phat, fat_sector_LBA, &cached_sector);
	if (ret != PhatState_OK) return ret;
	size_t ent_offset_in_sector = fat_offset % phat->bytes_per_sector;
	switch (phat->FAT_bits)
	{
	case 12:
		raw_entry = *(uint16_t *)&cached_sector->data[ent_offset_in_sector];
		if (half_cluster == 0)
			cluster_number = raw_entry & 0x0FFF;
		else
			cluster_number = (raw_entry >> 4) & 0x0FFF;
		if (cluster_number >= 0xFF8) return PhatState_EndOfFATChain;
		if (cluster_number >= 0xFF0) return PhatState_FATError;
		if (cluster_number < 2) return PhatState_FATError;
		break;
	case 16:
		cluster_number = *(uint16_t *)&cached_sector->data[ent_offset_in_sector];
		if (cluster_number >= 0xFFF8) return PhatState_EndOfFATChain;
		if (cluster_number >= 0xFFF0) return PhatState_FATError;
		if (cluster_number < 2) return PhatState_FATError;
		break;
	case 32:
		cluster_number = *(uint32_t *)&cached_sector->data[ent_offset_in_sector];
		if (cluster_number >= 0x0FFFFFF8) return PhatState_EndOfFATChain;
		if (cluster_number >= 0x0FFFFFF0) return PhatState_FATError;
		if (cluster_number < 2) return PhatState_FATError;
		break;
	default:
		return PhatState_InternalError;
	}
	*next_cluster = cluster_number;
	return PhatState_OK;
}
