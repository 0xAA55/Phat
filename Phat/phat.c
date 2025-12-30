#include "phat.h"

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
			*pp_cached_sector = cached_sector;
			return PhatState_OK;
		}
	}

	for (size_t i = 0; i < PHAT_CACHED_SECTORS; i++)
	{
		Phat_SectorCache_p cached_sector = &phat->cache[i];
		int MustDo = (i == PHAT_CACHED_SECTORS - 1);
		if (MustDo || !Phat_IsCachedSectorValid(cached_sector) || phat->LRU_age - Phat_GetCachedSectorAge(cached_sector) >= PHAT_CACHED_SECTORS)
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
	LBA_t end_of_FAT_LBA;
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

static LBA_t Phat_ClusterToLBA(Phat_p phat, uint32_t cluster)
{
	return phat->data_start_LBA + (LBA_t)(cluster - 2) * phat->sectors_per_cluster;
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

static uint8_t Phat_LFN_ChkSum(uint8_t *file_name_8_3)
{
	uint8_t sum = 0;
	for (int i = 0; i < 11; i++) sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + *file_name_8_3++;
	return sum;
}

static PhatState Phat_GetDirItem(Phat_p phat, Phat_DirInfo_p dir_info, Phat_DirItem_p dir_item)
{
	PhatState ret = PhatState_OK;
	LBA_t dir_sector_LBA;
	uint32_t item_index_in_sector;
	Phat_SectorCache_p cached_sector;
	Phat_DirItem_p dir_items;

	if (dir_info->dir_current_cluster >= 2)
		dir_sector_LBA = Phat_ClusterToLBA(phat, dir_info->dir_current_cluster) + dir_info->cur_diritem_in_cur_cluster / phat->num_diritems_in_a_sector;
	else // Root directory in FAT12/16
		dir_sector_LBA = phat->root_dir_start_LBA + dir_info->cur_diritem_in_cur_cluster / phat->num_diritems_in_a_sector;
	item_index_in_sector = dir_info->cur_diritem_in_cur_cluster % phat->num_diritems_in_a_sector;
	dir_sector_LBA += phat->partition_start_LBA;
	ret = Phat_ReadSectorThroughCache(phat, dir_sector_LBA, &cached_sector);
	if (ret != PhatState_OK) return ret;
	dir_items = (Phat_DirItem_p)&cached_sector->data[0];
	*dir_item = dir_items[item_index_in_sector];
	return PhatState_OK;
}

static void Phat_SuckLFNIntoBuffer(Phat_LFN_Entry_p lfn_item, Phat_DirInfo_p buffer)
{
	if (buffer->LFN_length == MAX_LFN) return;
	for (size_t i = 0; i < 5; i++)
	{
		WChar_t wchar = lfn_item->name1[i];
		buffer->LFN_name[buffer->LFN_length++] = wchar;
		if (!wchar) return;
		if (buffer->LFN_length == MAX_LFN) return;
	}
	for (size_t i = 0; i < 6; i++)
	{
		WChar_t wchar = lfn_item->name2[i];
		buffer->LFN_name[buffer->LFN_length++] = wchar;
		if (!wchar) return;
		if (buffer->LFN_length == MAX_LFN) return;
	}
	for (size_t i = 0; i < 2; i++)
	{
		WChar_t wchar = lfn_item->name3[i];
		buffer->LFN_name[buffer->LFN_length++] = wchar;
		if (!wchar) return;
		if (buffer->LFN_length == MAX_LFN) return;
	}
}

static PhatBool_t Phat_IsValidLFNEntry(Phat_DirItem_p lfn_item)
{
	Phat_LFN_Entry_p lfne = (Phat_LFN_Entry_p)lfn_item;
	if (lfne->attributes != ATTRIB_LFN) return 0;
	if (lfne->type != 0) return 0;
	if (lfne->first_cluster_low != 0) return 0;
	return 1;
}

static PhatState Phat_MoveToNextDirItem(Phat_p phat, Phat_DirInfo_p dir_info)
{
	PhatState ret = PhatState_OK;
	uint32_t next_cluster;
	if (dir_info->cur_diritem_in_cur_cluster++ >= phat->num_diritems_in_a_cluster)
	{
		ret = Phat_GetFATNextCluster(phat, dir_info->dir_current_cluster, &next_cluster);
		if (ret == PhatState_OK)
		{
			dir_info->cur_diritem_in_cur_cluster = 0;
			dir_info->dir_current_cluster = next_cluster;
		}
		else if (ret == PhatState_EndOfFATChain)
		{
			dir_info->cur_diritem_in_cur_cluster--;
			return PhatState_EndOfDirectory;
		}
		else
		{
			return ret;
		}
	}
	return ret;
}

PhatState Phat_NextDirItem(Phat_p phat, Phat_DirInfo_p dir_info)
{
	PhatState ret = PhatState_OK;
	Phat_DirItem_t diritem;
	Phat_LFN_Entry_p lfnitem;
	PhatBool_t no_checksum = 1;
	uint8_t checksum;

	dir_info->LFN_length = 0;
	for (;;)
	{
		ret = Phat_GetDirItem(phat, dir_info, &diritem);
		if (ret != PhatState_OK) return ret;
		if (Phat_IsValidLFNEntry(&diritem))
		{
			lfnitem = (Phat_LFN_Entry_p)&diritem;
			if (no_checksum)
			{
				no_checksum = 0;
				dir_info->checksum = lfnitem->checksum;
			}
			else if (lfnitem->checksum != dir_info->checksum)
			{
				dir_info->checksum = lfnitem->checksum;
				dir_info->LFN_length = 0;
			}
			Phat_SuckLFNIntoBuffer(lfnitem, dir_info);
			ret = Phat_MoveToNextDirItem(phat, dir_info);
			if (ret != PhatState_OK) return ret;
		}
		else if (diritem.file_name_8_3[0] == 0xE5)
		{
			dir_info->LFN_length = 0;
			no_checksum = 1;
			ret = Phat_MoveToNextDirItem(phat, dir_info);
			if (ret != PhatState_OK) return ret;
		}
		else if (diritem.file_name_8_3[0] == 0x00)
		{
			dir_info->LFN_length = 0;
			return PhatState_EndOfDirectory;
		}
		else
		{
			checksum = Phat_LFN_ChkSum(diritem.file_name_8_3);
			if (!no_checksum && checksum != dir_info->checksum)
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
				size_t copy_to = 0;
				for (size_t i = 0; i < 8; i++)
				{
					uint8_t ch = diritem.file_name_8_3[i];
					if (!ch) break;
					if (ch != ' ') copy_to = i;
				}
				for (size_t i = 0; i < copy_to; i++)
				{
					dir_info->LFN_name[dir_info->LFN_length++] = Cp437_To_Unicode(diritem.file_name_8_3[i]);
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
				for (size_t i = 8; i < copy_to; i++)
				{
					dir_info->LFN_name[dir_info->LFN_length++] = Cp437_To_Unicode(diritem.file_name_8_3[i]);
				}
				dir_info->LFN_name[dir_info->LFN_length] = L'\0';
			}
			Phat_MoveToNextDirItem(phat, dir_info);
			return PhatState_OK;
		}
	}
}

PhatState Phat_CloseDir(Phat_p phat, Phat_DirInfo_p dir_info)
{
	UNUSED(phat);
	memset(dir_info, 0, sizeof * dir_info);
	return PhatState_OK;
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
	while (*read_ptr)
	{
		if (*read_ptr == L'/' || *read_ptr == L'\\')
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
		}
		read_ptr++;
	}
	*write_ptr = L'\0';
	if (write_ptr > path && (*(write_ptr - 1) == L'/' || *(write_ptr - 1) == L'\\'))
	{
		*(write_ptr - 1) = L'\0';
	}
}

PhatState Phat_OpenDir(Phat_p phat, WChar_p path, Phat_DirInfo_p dir_info)
{
	LBA_t cur_dir_sector = phat->root_dir_start_LBA;
	uint32_t cur_dir_cluster = phat->root_dir_cluster;
	WChar_p ptr = path;
	WChar_p name_start;
	size_t name_len;
	PhatState ret = PhatState_OK;

	Phat_NormalizePath(path);
	memset(dir_info, 0, sizeof * dir_info);
	dir_info->first_cluster = cur_dir_cluster;

	for (;;)
	{
		name_start = ptr;
		while (*ptr != L'\0' && *ptr != L'/') ptr++;
		name_len = (size_t)(ptr - name_start);
		if (name_len > MAX_LFN) return PhatState_InvalidPath;

		if (*ptr) // is middle path
		{
			dir_info->dir_start_cluster = cur_dir_cluster;
			dir_info->dir_current_cluster = cur_dir_cluster;
			dir_info->cur_diritem_in_cur_cluster = 0;
			for (;;)
			{
				ret = Phat_NextDirItem(phat, dir_info);
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
			dir_info->cur_diritem_in_cur_cluster = 0;
			return PhatState_OK;
		}
	}
}
