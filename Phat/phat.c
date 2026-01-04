#include "phat.h"

#include <ctype.h>
#include <string.h>

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

#pragma pack(push, 1)
typedef struct Phat_CHS_s
{
	uint8_t head;
	uint8_t sector;
	uint8_t cylinder;
}Phat_CHS_t, *Phat_CHS_p;

typedef struct Phat_MBR_Entry_s
{
	uint8_t boot_indicator;
	Phat_CHS_t starting_chs;
	uint8_t partition_type;
	Phat_CHS_t ending_chs;
	uint32_t starting_LBA;
	uint32_t size_in_sectors;
}Phat_MBR_Entry_t, *Phat_MBR_Entry_p;

typedef struct Phat_MBR_s
{
	uint8_t boot_code[446];
	Phat_MBR_Entry_t partition_entries[4];
	uint16_t boot_signature;
}Phat_MBR_t, *Phat_MBR_p;

typedef struct Phat_DBR_FAT_s
{
	uint8_t jump_boot[3];
	uint8_t OEM_name[8];
	uint16_t bytes_per_sector;
	uint8_t sectors_per_cluster;
	uint16_t reserved_sector_count;
	uint8_t num_FATs;
	uint16_t root_dir_entry_count;
	uint16_t total_sectors_16;
	uint8_t media;
	uint16_t FAT_size;
	uint16_t sectors_per_track;
	uint16_t num_heads;
	uint32_t hidden_sectors;
	uint32_t total_sectors_32;
	uint8_t BIOS_drive_number;
	uint8_t first_head;
	uint8_t extension_flag;
	uint32_t volume_ID;
	uint8_t volume_label[11];
	uint8_t file_system_type[8];
	uint8_t boot_code[448];
	uint16_t boot_sector_signature;
}Phat_DBR_FAT_t, *Phat_DBR_FAT_p;

typedef struct Phat_DBR_FAT32_s
{
	uint8_t jump_boot[3];
	uint8_t OEM_name[8];
	uint16_t bytes_per_sector;
	uint8_t sectors_per_cluster;
	uint16_t reserved_sector_count;
	uint8_t num_FATs;
	uint16_t root_dir_entry_count;
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
}Phat_DBR_FAT32_t, *Phat_DBR_FAT32_p;

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

static const uint8_t empty_sector[512] = { 0 };

static PhatState Phat_ReadFAT(Phat_p phat, Cluster_t cluster, Cluster_t *read_out);
static PhatState Phat_WriteFAT(Phat_p phat, Cluster_t cluster, Cluster_t write, PhatBool_t flush);

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

const char *Phat_StateToString(PhatState s)
{
	static const char *strlist[] =
	{
		"OK",
		"Invalid parameter",
		"Internal error",
		"Driver error",
		"Read fail",
		"Write fail",
		"Partition table error",
		"Filesystem is not FAT",
		"FAT error",
		"Filesystem error",
		"File not found",
		"Directory not found",
		"Is a directory",
		"Not a directory",
		"Invalid path",
		"End of directory",
		"End of FAT chain",
		"End of file",
		"Not enough space",
		"Directory is not empty",
		"File is already exists",
		"Directory is already exists",
		"The file is opened in RO mode",
		"The name is too long",
		"The name contains invalid characters",
		"Can not format the partition because parameters are insane",
		"The current partition formatting approach is suboptimal, resulting in significant space wastage",
		"The disk is already initialized",
		"No free partitions to allocate",
		"The partition is too small for FAT FS",
		"The first sector does NOT appear to be a MBR",
		"The disk is using GPT, currently not supported",
		"The partition overlaps an existing one",
	};
	if (s >= PhatState_LastState) return "InvalidStateNumber";
	else return strlist[s];
}

static WChar_p Phat_ToEndOfString(WChar_p string)
{
	while (*string) string++;
	return string;
}

static size_t Phat_Wcslen(const WChar_p str)
{
	return (size_t)(Phat_ToEndOfString(str) - str);
}

static WChar_p Phat_Wcscpy(WChar_p dest, const WChar_p src)
{
	size_t len = Phat_Wcslen(src) + 1;
	return memmove(dest, src, len * sizeof(WChar_t));
}

static WChar_p Phat_Wcsncpy(WChar_p dest, const WChar_p src, size_t length)
{
	size_t len = Phat_Wcslen(src) + 1;
	if (len > length) len = length;
	return memmove(dest, src, len * sizeof(WChar_t));
}

static int Phat_Wcscmp(WChar_p s1, const WChar_p s2)
{
	size_t len1 = Phat_Wcslen(s1);
	size_t len2 = Phat_Wcslen(s2);
	if (len1 > len2) return 1;
	if (len2 > len1) return -1;
	return memcmp(s1, s2, len1 * sizeof(WChar_t));
}

void Phat_ToUpperDirectoryPath(WChar_p path)
{
	size_t length = 0;
	PhatBool_t tail_trimming = 1;

	// Check parameters
	if (!path) return;

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

void Phat_NormalizePath(WChar_p path)
{
	WChar_p read_ptr;
	WChar_p write_ptr;
	WChar_p start_ptr;
	size_t length;

	// Check parameters
	if (!path) return;

	read_ptr = path;
	write_ptr = path;
	while (*read_ptr == '/' || *read_ptr == '\\') read_ptr++;
	if (read_ptr > path && (*(read_ptr - 1) == '/' || *(read_ptr - 1) == '\\')) read_ptr--;
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
	WChar_p chr;
	size_t length = 0;

	// Check parameters
	if (!path || !name);

	chr = Phat_ToEndOfString(path);
	// Remove trailing slashes
	while (chr > path)
	{
		if (*chr == L'/' || *chr == L'\\')
			*chr-- = L'\0';
		else
			break;
	}
	// Get trailing filename start position and name
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
	// Make sure `chr` points to the name
	if (*chr == L'/' || *chr == L'\\')
	{
		chr++;
		length--;
	}
	if (length > MAX_LFN) length = MAX_LFN;
	for (size_t i = 0; i <= length; i++)
	{
		name[i] = chr[i];
	}
}

void Phat_PathToNameInPlace(WChar_p path)
{
	Phat_PathToName(path, path);
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
	PhatState ret;
	if (Phat_IsCachedSectorValid(cached_sector) && !Phat_IsCachedSectorSync(cached_sector))
	{
		if (!phat->write_enable) return PhatState_InternalError;
		ret = Phat_WriteBackCachedSector(phat, cached_sector);
		if (ret != PhatState_OK) return ret;
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
	return PhatState_OK;
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
	return PhatState_OK;
}

static LBA_t Phat_CHS_to_LBA(Phat_CHS_p chs)
{
	uint8_t actual_sector = chs->sector & 0x1F;
	uint16_t actual_cylinder = ((uint16_t)(chs->sector & 0xC0) << 2) | chs->cylinder;
	if (actual_sector < 1) return 0;
	return ((LBA_t)actual_cylinder * 255 + chs->head) * 63 + (actual_sector - 1);
}

static PhatBool_t Phat_LBA_to_CHS(LBA_t LBA, Phat_CHS_p chs)
{
	const uint16_t heads_per_cylinder = 255;
	const uint8_t sectors_per_track = 63;

	if (LBA >= 1024 * heads_per_cylinder * sectors_per_track)
	{
		// Overflowed, set to maximum
		chs->head = 0xFE;
		chs->sector = 0xFF;
		chs->cylinder = 0xFF;
		return 0;
	}
	else
	{
		uint16_t cylinder = LBA / (heads_per_cylinder * sectors_per_track);
		uint8_t head = (LBA / sectors_per_track) % heads_per_cylinder;
		uint8_t sector = (LBA % sectors_per_track) + 1;

		chs->head = head;
		chs->sector = (uint8_t)(sector | ((cylinder >> 2) & 0xC0));
		chs->cylinder = (uint8_t)(cylinder & 0xFF);
		return 1;
	}
}

static PhatBool_t Phat_GetMBREntryInfo(Phat_MBR_Entry_p entry, LBA_t *p_starting_LBA, LBA_t *p_size_in_sectors)
{
	LBA_t starting_LBA_from_CHS = Phat_CHS_to_LBA(entry->starting_chs.head, entry->starting_chs.sector, entry->starting_chs.cylinder);
	LBA_t ending_LBA_from_CHS = Phat_CHS_to_LBA(entry->ending_chs.head, entry->ending_chs.sector, entry->ending_chs.cylinder);
	LBA_t size_in_sectors_from_CHS = ending_LBA_from_CHS - starting_LBA_from_CHS;
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

static PhatBool_t Phat_IsSectorDBR(const Phat_DBR_FAT32_p dbr)
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
	if (Phat_IsSectorDBR((Phat_DBR_FAT32_p)mbr)) return 0;
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
	// Check parameters
	if (!phat) return PhatState_InvalidParameter;
	static const Phat_Date_t default_date =
	{
		PHAT_DEFAULT_YEAR,
		PHAT_DEFAULT_MONTH,
		PHAT_DEFAULT_DAY,
	};
	static const Phat_Time_t default_time =
	{
		PHAT_DEFAULT_HOUR,
		PHAT_DEFAULT_MINUTE,
		PHAT_DEFAULT_SECOND,
		0,
	};
	memset(phat, 0, sizeof * phat);
	phat->driver = Phat_InitDriver(NULL);
	if (!Phat_OpenDevice(&phat->driver)) return PhatState_DriverError;
	Phat_SetCurDateTime(phat, (const Phat_Date_p)&default_date, (const Phat_Time_p)&default_time);
	return PhatState_OK;
}

// Iterate through FAT to find a free cluster
static PhatState Phat_SearchForFreeCluster(Phat_p phat, Cluster_t from, Cluster_t *cluster_out)
{
	PhatState ret;
	Cluster_t cluster;
	for (Cluster_t i = from; i < phat->num_FAT_entries; i++)
	{
		ret = Phat_ReadFAT(phat, i, &cluster);
		if (ret != PhatState_OK) return ret;
		if (!cluster)
		{
			*cluster_out = i;
			return PhatState_OK;
		}
	}
	return PhatState_NotEnoughSpace;
}

// Iterate through FAT to count free clusters
static PhatState Phat_SumFreeClusters(Phat_p phat, Cluster_t *num_free_clusters_out)
{
	PhatState ret;
	Cluster_t cluster;
	Cluster_t sum = 0;
	for (Cluster_t i = 0; i < phat->num_FAT_entries; i++)
	{
		ret = Phat_ReadFAT(phat, i, &cluster);
		if (ret != PhatState_OK) return ret;
		if (!cluster) sum++;
	}
	*num_free_clusters_out = sum;
	return PhatState_OK;
}

static LBA_t Phat_ClusterToLBA(Phat_p phat, Cluster_t cluster)
{
	return phat->data_start_LBA + (LBA_t)(cluster - 2) * phat->sectors_per_cluster;
}

// Find next free cluster starting from phat->next_free_cluster
static PhatState Phat_SeekForFreeCluster(Phat_p phat, Cluster_t *cluster_out)
{
	return Phat_SearchForFreeCluster(phat, phat->next_free_cluster, cluster_out);
}

static PhatState Phat_MarkDirty(Phat_p phat, PhatBool_t is_dirty, PhatBool_t flush_immediately)
{
	PhatState ret;
	Cluster_t clean_bit;
	Cluster_t dirty_entry;

	switch(phat->FAT_bits)
	{
	case 12: clean_bit = 0x800; break;
	case 16: clean_bit = 0x8000; break;
	case 32: clean_bit = 0x80000000; break;
	default: return PhatState_InternalError;
	}

	ret = Phat_ReadFAT(phat, 0, &dirty_entry);
	if (ret != PhatState_OK) return ret;
	if (is_dirty) dirty_entry &= clean_bit - 1;
	else dirty_entry |= clean_bit;
	ret = Phat_WriteFAT(phat, 0, dirty_entry, 1);
	if (ret != PhatState_OK) return ret;

	return PhatState_OK;
}

static PhatState Phat_CheckIsDirty(Phat_p phat, PhatBool_t *is_dirty)
{
	PhatState ret;
	Cluster_t clean_bit;
	Cluster_t dirty_entry;

	switch (phat->FAT_bits)
	{
	case 12: clean_bit = 0x800; break;
	case 16: clean_bit = 0x8000; break;
	case 32: clean_bit = 0x80000000; break;
	default: return PhatState_InternalError;
	}

	ret = Phat_ReadFAT(phat, 0, &dirty_entry);
	if (ret != PhatState_OK) return ret;
	if (dirty_entry & clean_bit)*is_dirty = 0;
	else *is_dirty = 1;

	return PhatState_OK;
}

// Open a partition, load all of the informations from the DBR in order to manipulate files/directories
PhatState Phat_Mount(Phat_p phat, int partition_index, PhatBool_t write_enable)
{
	LBA_t partition_start_LBA = 0;
	LBA_t total_sectors = 0;
	LBA_t end_of_FAT_LBA;
	PhatState ret = PhatState_OK;
	Phat_SectorCache_p cached_sector;
	Phat_MBR_p mbr;
	Phat_DBR_FAT_p dbr;
	Phat_DBR_FAT32_p dbr_32;
	Phat_FSInfo_p fsi;

	// Check parameters
	if (!phat) return PhatState_InvalidParameter;

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

	phat->write_enable = write_enable;

	dbr = (Phat_DBR_FAT_p)cached_sector->data;
	dbr_32 = (Phat_DBR_FAT32_p)cached_sector->data;
	if (!Phat_IsSectorDBR(dbr_32)) return PhatState_FSNotFat;
	if (!memcmp(dbr->file_system_type, "FAT12   ", 8))
		phat->FAT_bits = 12;
	else if (!memcmp(dbr->file_system_type, "FAT16   ", 8))
		phat->FAT_bits = 16;
	else if (!memcmp(dbr_32->file_system_type, "FAT32   ", 8))
		phat->FAT_bits = 32;
	else
		return PhatState_FSNotFat;
	phat->partition_start_LBA = partition_start_LBA;
	if (phat->FAT_bits != 32)
	{
		phat->FAT_size_in_sectors = dbr->FAT_size;
		end_of_FAT_LBA = dbr->reserved_sector_count + (LBA_t)dbr->num_FATs * phat->FAT_size_in_sectors;
		phat->total_sectors = dbr->total_sectors_16 ? dbr->total_sectors_16 : dbr->total_sectors_32;
		phat->num_FATs = dbr->num_FATs;
		phat->FAT1_start_LBA = dbr->reserved_sector_count;
		phat->root_dir_cluster = 0;
		phat->root_dir_start_LBA = end_of_FAT_LBA;
		phat->root_dir_entry_count = dbr->root_dir_entry_count;
		phat->data_start_LBA = phat->root_dir_start_LBA + (((LBA_t)dbr->root_dir_entry_count * 32) + (dbr->bytes_per_sector - 1)) / dbr->bytes_per_sector;
		phat->bytes_per_sector = dbr->bytes_per_sector;
		phat->sectors_per_cluster = dbr->sectors_per_cluster;
		phat->num_diritems_in_a_sector = phat->bytes_per_sector / 32;
		phat->num_diritems_in_a_cluster = (phat->bytes_per_sector * phat->sectors_per_cluster) / 32;
		phat->num_FAT_entries = (phat->FAT_size_in_sectors * phat->bytes_per_sector * 8) / phat->FAT_bits;
		phat->FATs_are_same = 1;
		switch (phat->FAT_bits)
		{
		case 12: phat->end_of_cluster_chain = 0x0FF8; break;
		case 16: phat->end_of_cluster_chain = 0xFFF8; break;
		}
		phat->max_valid_cluster = phat->num_FAT_entries + 1;
	}
	else
	{
		phat->FAT_size_in_sectors = dbr_32->FAT_size_16 ? dbr_32->FAT_size_16 : dbr_32->FAT_size_32;
		end_of_FAT_LBA = dbr_32->reserved_sector_count + (LBA_t)dbr_32->num_FATs * phat->FAT_size_in_sectors;
		phat->total_sectors = dbr_32->total_sectors_16 ? dbr_32->total_sectors_16: dbr_32->total_sectors_32;
		phat->num_FATs = dbr_32->num_FATs;
		phat->FAT1_start_LBA = dbr_32->reserved_sector_count;
		phat->root_dir_cluster = dbr_32->root_dir_cluster;
		phat->root_dir_start_LBA = end_of_FAT_LBA + (LBA_t)(phat->root_dir_cluster - 2) * dbr_32->sectors_per_cluster;
		phat->root_dir_entry_count = dbr_32->root_dir_entry_count;
		phat->data_start_LBA = phat->root_dir_start_LBA;
		phat->bytes_per_sector = dbr_32->bytes_per_sector;
		phat->sectors_per_cluster = dbr_32->sectors_per_cluster;
		phat->num_diritems_in_a_sector = phat->bytes_per_sector / 32;
		phat->num_diritems_in_a_cluster = (phat->bytes_per_sector * phat->sectors_per_cluster) / 32;
		phat->num_FAT_entries = (phat->FAT_size_in_sectors * phat->bytes_per_sector * 8) / phat->FAT_bits;
		phat->FATs_are_same = !dbr_32->FATs_are_different;
		phat->end_of_cluster_chain = 0x0FFFFFF8;
		phat->max_valid_cluster = phat->num_FAT_entries + 1;

		// Read FSInfo sector
		ret = Phat_ReadSectorThroughCache(phat, partition_start_LBA + dbr_32->FS_info_sector, &cached_sector);
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
	}

	ret = Phat_CheckIsDirty(phat, &phat->is_dirty);
	if (ret != PhatState_OK) return ret;

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

	// Check parameters
	if (!phat) return PhatState_InvalidParameter;

	for (size_t i = 0; i < PHAT_CACHED_SECTORS; i++)
	{
		Phat_SectorCache_p cached_sector = &phat->cache[i];
		ret = Phat_InvalidateCachedSector(phat, cached_sector);
		if (ret != PhatState_OK) return ret;
	}
	return ret;
}

PhatState Phat_Unmount(Phat_p phat)
{
	PhatState ret;

	// Check parameters
	if (!phat) return PhatState_InvalidParameter;

	ret = Phat_FlushCache(phat);
	if (ret != PhatState_OK) return ret;

	if (phat->write_enable)
	{
		ret = Phat_MarkDirty(phat, phat->is_dirty, 1);
		if (ret != PhatState_OK) return ret;
	}
	return PhatState_OK;
}

PhatState Phat_DeInit(Phat_p phat)
{
	PhatState ret;

	// Check parameters
	if (!phat) return PhatState_InvalidParameter;

	ret = Phat_Unmount(phat);
	if (ret != PhatState_OK) return ret;
	if (!Phat_CloseDevice(&phat->driver)) return PhatState_DriverError;
	Phat_DeInitDriver(&phat->driver);
	memset(phat, 0, sizeof * phat);
	return PhatState_OK;
}

void Phat_SetCurDateTime(Phat_p phat, const Phat_Date_p cur_date, const Phat_Time_p cur_time)
{
	// Check parameters
	if (!phat || (!cur_date && !cur_time)) return;
	if (cur_date) phat->cur_date = *cur_date;
	if (cur_time) phat->cur_time = *cur_time;
}

static PhatState Phat_WipeCluster(Phat_p phat, Cluster_t cluster)
{
	PhatState ret;
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

// Read FAT table by `cluster` starting from 0
static PhatState Phat_ReadFAT(Phat_p phat, Cluster_t cluster, Cluster_t *read_out)
{
	PhatState ret = PhatState_OK;
	uint16_t raw_entry;
	int half_cluster = 0;
	LBA_t fat_offset;
	LBA_t fat_sector_LBA;
	Phat_SectorCache_p cached_sector;
	Cluster_t cluster_number;
	size_t ent_offset_in_sector;

	switch (phat->FAT_bits)
	{
	case 12:
		half_cluster = cluster & 1;
		fat_offset = (LBA_t)cluster + (cluster >> 1);
		break;
	case 16:
		fat_offset = (LBA_t)cluster * 2;
		break;
	case 32:
		fat_offset = (LBA_t)cluster * 4;
		break;
	default:
		return PhatState_InternalError;
	}
	fat_sector_LBA = phat->partition_start_LBA + phat->FAT1_start_LBA + (fat_offset / phat->bytes_per_sector);
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

// Write FAT table by `cluster` starting from 0
static PhatState Phat_WriteFAT(Phat_p phat, Cluster_t cluster, Cluster_t write, PhatBool_t flush)
{
	PhatState ret = PhatState_OK;
	int half_cluster = 0;
	LBA_t fat_offset;
	LBA_t fat_sector_LBA;
	Phat_SectorCache_p cached_sector;
	size_t ent_offset_in_sector;

	switch (phat->FAT_bits)
	{
	case 12:
		half_cluster = cluster & 1;
		fat_offset = (LBA_t)cluster + (cluster >> 1);
		break;
	case 16:
		fat_offset = (LBA_t)cluster * 2;
		break;
	case 32:
		fat_offset = (LBA_t)cluster * 4;
		break;
	default:
		return PhatState_InternalError;
	}
	for (LBA_t i = 0; i < phat->num_FATs; i++)
	{
		fat_sector_LBA = phat->partition_start_LBA + phat->FAT1_start_LBA + i * phat->FAT_size_in_sectors + (fat_offset / phat->bytes_per_sector);
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
		if (flush) Phat_WriteBackCachedSector(phat, cached_sector);
		if (!phat->FATs_are_same) break;
	}
	return PhatState_OK;
}

static PhatState Phat_UnlinkCluster(Phat_p phat, Cluster_t cluster)
{
	PhatState ret;
	Cluster_t next_sector;
	Cluster_t end_of_chain = phat->end_of_cluster_chain;
	for (;;)
	{
		if (cluster < phat->next_free_cluster) phat->next_free_cluster = cluster;
		ret = Phat_ReadFAT(phat, cluster, &next_sector);
		if (ret != PhatState_OK) return ret;
		ret = Phat_WriteFAT(phat, cluster, 0, 0);
		if (ret != PhatState_OK) return ret;
		if (next_sector >= end_of_chain) break;
		if (next_sector < 2 || next_sector > phat->max_valid_cluster) return PhatState_FATError;
		cluster = next_sector;
	}
	return PhatState_OK;
}

// `allocated_cluster` could point to a valid cluster that you wish to allocate
static PhatState Phat_AllocateCluster(Phat_p phat, Cluster_t *allocated_cluster)
{
	PhatState ret;
	Cluster_t free_cluster = *allocated_cluster;
	if (free_cluster >= 2 && free_cluster <= phat->max_valid_cluster)
	{
		Cluster_t value;
		ret = Phat_ReadFAT(phat, free_cluster, &value);
		if (ret == PhatState_OK && value == 0)
		{
			ret = Phat_WriteFAT(phat, free_cluster, phat->end_of_cluster_chain, 0);
			if (ret != PhatState_OK) return ret;
			if (phat->has_FSInfo)
			{
				if (phat->next_free_cluster == free_cluster)
				{
					ret = Phat_SearchForFreeCluster(phat, free_cluster + 1, &phat->next_free_cluster);
					if (ret != PhatState_OK)
					{
						phat->next_free_cluster = 2;
					}
				}
				if (phat->free_clusters > 0)
					phat->free_clusters--;
			}
		}
	}
	ret = Phat_SeekForFreeCluster(phat, &free_cluster);
	if (ret != PhatState_OK) return ret;
	ret = Phat_WriteFAT(phat, free_cluster, phat->end_of_cluster_chain, 0);
	if (ret != PhatState_OK) return ret;
	*allocated_cluster = free_cluster;
	if (phat->has_FSInfo)
	{
		if (free_cluster == phat->next_free_cluster)
		{
			ret = Phat_SearchForFreeCluster(phat, free_cluster + 1, &phat->next_free_cluster);
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
static PhatState Phat_GetFATNextCluster(Phat_p phat, Cluster_t cur_cluster, Cluster_t *next_cluster)
{
	PhatState ret = PhatState_OK;
	Cluster_t cluster_number;
	if (cur_cluster < 2) return PhatState_InvalidParameter;
	if (cur_cluster > phat->max_valid_cluster) return PhatState_InvalidParameter;
	ret = Phat_ReadFAT(phat, cur_cluster, &cluster_number);
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

static PhatState Phat_UpdateClusterByDirItemIndex(Phat_DirInfo_p dir_info, PhatBool_t allocate_new_sectors)
{
	PhatState ret = PhatState_OK;
	Phat_p phat = dir_info->phat;
	Cluster_t cluster_index;
	Cluster_t next_cluster;

	if (phat->FAT_bits != 32)
	{
		if (dir_info->dir_start_cluster == 0)
		{
			// For root dir, no cluster need to be updated
			if (dir_info->cur_diritem >= phat->root_dir_entry_count) return PhatState_EndOfDirectory;
			return PhatState_OK;
		}
	}
	else
	{
		if (dir_info->dir_start_cluster < 2) return PhatState_InvalidParameter;
	}
	cluster_index = dir_info->cur_diritem / phat->num_diritems_in_a_cluster;
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
		{
			if (!allocate_new_sectors) return PhatState_EndOfDirectory;
			// Allocate a new cluster
			next_cluster = dir_info->dir_current_cluster + 1;
			ret = Phat_AllocateCluster(phat, &next_cluster);
			if (ret != PhatState_OK) return ret;
			ret = Phat_WriteFAT(phat, dir_info->dir_current_cluster, next_cluster, 0);
			if (ret != PhatState_OK) return ret;
			dir_info->dir_current_cluster = next_cluster;
			ret = Phat_WipeCluster(phat, next_cluster);
			if (ret != PhatState_OK) return ret;
		}
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
	uint8_t item_index_in_sector;
	Phat_SectorCache_p cached_sector;
	Phat_DirItem_p dir_items;
	Phat_p phat = dir_info->phat;
	uint16_t cur_diritem_in_cur_cluster = dir_info->cur_diritem % phat->num_diritems_in_a_cluster;

	if (phat->FAT_bits == 32)
	{
		if (dir_info->dir_start_cluster == 0) return PhatState_EndOfDirectory;
		ret = Phat_UpdateClusterByDirItemIndex(dir_info, 0);
		if (ret != PhatState_OK) return ret;
		dir_sector_LBA = Phat_ClusterToLBA(phat, dir_info->dir_current_cluster) + cur_diritem_in_cur_cluster / phat->num_diritems_in_a_sector;
	}
	else // Root directory in FAT12/16
	{
		if (dir_info->dir_start_cluster == 0)
			dir_sector_LBA = phat->root_dir_start_LBA + dir_info->cur_diritem / phat->num_diritems_in_a_sector;
		else
		{
			ret = Phat_UpdateClusterByDirItemIndex(dir_info, 0);
			if (ret != PhatState_OK) return ret;
			dir_sector_LBA = Phat_ClusterToLBA(phat, dir_info->dir_current_cluster) + cur_diritem_in_cur_cluster / phat->num_diritems_in_a_sector;
		}
	}
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
	uint8_t item_index_in_sector;
	Phat_SectorCache_p cached_sector;
	Phat_DirItem_p dir_items;
	Phat_p phat = dir_info->phat;
	uint16_t cur_diritem_in_cur_cluster = dir_info->cur_diritem % phat->num_diritems_in_a_cluster;

	if (phat->FAT_bits == 32)
	{
		if (dir_info->dir_start_cluster == 0) return PhatState_EndOfDirectory;
		ret = Phat_UpdateClusterByDirItemIndex(dir_info, 0);
		if (ret != PhatState_OK) return ret;
		dir_sector_LBA = Phat_ClusterToLBA(phat, dir_info->dir_current_cluster) + cur_diritem_in_cur_cluster / phat->num_diritems_in_a_sector;
	}
	else // Root directory in FAT12/16
	{
		if (dir_info->dir_start_cluster == 0)
			dir_sector_LBA = phat->root_dir_start_LBA + dir_info->cur_diritem / phat->num_diritems_in_a_sector;
		else
		{
			ret = Phat_UpdateClusterByDirItemIndex(dir_info, 0);
			if (ret != PhatState_OK) return ret;
			dir_sector_LBA = Phat_ClusterToLBA(phat, dir_info->dir_current_cluster) + cur_diritem_in_cur_cluster / phat->num_diritems_in_a_sector;
		}
	}
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
	if (lfn_item->order & 0x40) buffer->LFN_length = write_pos % 13 ? write_pos - 1 : write_pos;
	return PhatState_OK;
}

static PhatBool_t Phat_IsValidLFNEntry(Phat_DirItem_p lfn_item)
{
	Phat_LFN_Entry_p lfne = (Phat_LFN_Entry_p)lfn_item;
	if (lfne->order == 0xE5) return 0;
	if (lfne->attributes != ATTRIB_LFN) return 0;
	if (lfne->type != 0) return 0;
	if (lfne->first_cluster_low != 0) return 0;
	return 1;
}

static PhatState Phat_MoveToNextDirItem(Phat_DirInfo_p dir_info)
{
	dir_info->cur_diritem++;
	return Phat_UpdateClusterByDirItemIndex(dir_info, 0);
}

static PhatState Phat_MoveToNextDirItemWithAllocation(Phat_DirInfo_p dir_info)
{
	dir_info->cur_diritem++;
	return Phat_UpdateClusterByDirItemIndex(dir_info, 1);
}

PhatState Phat_NextDirItem(Phat_DirInfo_p dir_info)
{
	PhatState ret = PhatState_OK;
	Phat_DirItem_t diritem;
	Phat_LFN_Entry_p lfnitem;
	PhatBool_t no_checksum = 1;
	uint8_t checksum;
	Phat_p phat = dir_info->phat;

	// Check parameters
	if (!dir_info) return PhatState_InvalidParameter;

	lfnitem = (Phat_LFN_Entry_p)&diritem;
	dir_info->LFN_length = 0;
	for (;;)
	{
		ret = Phat_GetDirItem(dir_info, &diritem);
		if (ret != PhatState_OK) return ret;
		if (Phat_IsValidLFNEntry(&diritem))
		{
			// LFN entry
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

static PhatState Phat_FindFirstLFNEntry(Phat_DirInfo_p dir_info)
{
	PhatState ret;
	Phat_DirItem_t diritem;
	Phat_LFN_Entry_p lfnitem;
	uint8_t sfn_checksum;

	lfnitem = (Phat_LFN_Entry_p)&diritem;
	ret = Phat_GetDirItem(dir_info, &diritem);
	if (ret != PhatState_OK) return ret;
	if (Phat_IsValidLFNEntry(&diritem)) return PhatState_InvalidParameter;
	else if (diritem.file_name_8_3[0] == 0xE5)
	{
		// Deleted entry
		return PhatState_InvalidParameter;
	}
	else if (diritem.file_name_8_3[0] == 0x00)
	{
		// End of directory
		return PhatState_InvalidParameter;
	}
	else
	{
		sfn_checksum = Phat_LFN_ChkSum(diritem.file_name_8_3);
		for (;;)
		{
			dir_info->cur_diritem--;
			ret = Phat_GetDirItem(dir_info, &diritem);
			if (ret != PhatState_OK) return ret;
			if (lfnitem->attributes != ATTRIB_LFN || lfnitem->order == 0xE5)
			{
				dir_info->cur_diritem++;
				return PhatState_OK;
			}
			if (lfnitem->checksum != sfn_checksum || !Phat_IsValidLFNEntry(&diritem)) return PhatState_FSError;
			if (lfnitem->order & 0x40) return PhatState_OK;
		}
	}
}

void Phat_CloseDir(Phat_DirInfo_p dir_info)
{
	// Check parameters
	if (!dir_info) return;

	memset(dir_info, 0, sizeof * dir_info);
}

void Phat_OpenRootDir(Phat_p phat, Phat_DirInfo_p dir_info)
{
	// Check parameters
	if (!phat || !dir_info) return;

	memset(dir_info, 0, sizeof * dir_info);
	dir_info->phat = phat;
	dir_info->dir_start_cluster = phat->root_dir_cluster;
	dir_info->dir_current_cluster = phat->root_dir_cluster;
	dir_info->dir_current_cluster_index = 0;
	dir_info->cur_diritem = 0;
}

PhatState Phat_ChDir(Phat_DirInfo_p dir_info, const WChar_p dirname)
{
	PhatState ret;
	WChar_p dirname_ptr;
	WChar_p end_of_dirname = NULL;
	size_t dirname_len;

	if (!dir_info || !dirname) return PhatState_InvalidParameter;

	dirname_ptr = dirname;
	dir_info->cur_diritem = 0;

	for (;;)
	{
		if (end_of_dirname == NULL)
		{
			end_of_dirname = dirname_ptr;
			while (*end_of_dirname != 0 && *end_of_dirname != L'/' && *end_of_dirname != L'\\') end_of_dirname++;
			dirname_len = (size_t)(end_of_dirname - dirname_ptr);
			if (dirname_len == 0) return PhatState_InvalidParameter;
		}
		ret = Phat_NextDirItem(dir_info);
		if (ret == PhatState_OK)
		{
			if (dirname_len == dir_info->LFN_length &&
				!memcmp(dirname_ptr, dir_info->LFN_name, dirname_len * sizeof(WChar_t)))
			{
				Cluster_t dir_cluster;
				if ((dir_info->attributes & ATTRIB_DIRECTORY) == 0) return PhatState_NotADirectory;
				dir_cluster = dir_info->first_cluster;
				dir_info->dir_start_cluster = dir_cluster;
				dir_info->dir_current_cluster = dir_cluster;
				dir_info->dir_current_cluster_index = 0;
				dir_info->cur_diritem = 0;
				if (*end_of_dirname == 0) return PhatState_OK;
				end_of_dirname++;
				while (*end_of_dirname == L'/' || *end_of_dirname == L'\\')end_of_dirname++;
				dirname_ptr = end_of_dirname;
				end_of_dirname = NULL;
			}
		}
		else if (ret == PhatState_EndOfDirectory)
		{
			return PhatState_DirectoryNotFound;
		}
		else
		{
			return ret;
		}
	}
}

PhatState Phat_OpenDir(Phat_p phat, const WChar_p path, Phat_DirInfo_p dir_info)
{
	WChar_p ptr;
	if (!phat || !path || !dir_info) return PhatState_InvalidParameter;
	ptr = path;
	// Skip starting slash
	while (*ptr == L'/' || *ptr == L'\\') ptr++;
	Phat_OpenRootDir(phat, dir_info);
	if (*ptr)
		return Phat_ChDir(dir_info, ptr);
	else
		return PhatState_OK;
}

// Open a dir to the path, find the item if can (`PhatState_OK` will be returned), or return `PhatState_EndOfDirectory`
static PhatState Phat_FindItem(Phat_p phat, WChar_p path, Phat_DirInfo_p dir_info, WChar_p *next_path)
{
	PhatState ret;
	WChar_p dirname_ptr;
	WChar_p end_of_dirname = NULL;
	size_t dirname_len;

	if (!path) return PhatState_InvalidParameter;

	if (phat->FAT_bits == 32 && dir_info->dir_current_cluster < 2) Phat_OpenRootDir(phat, dir_info);

	dirname_ptr = path;
	dir_info->cur_diritem = 0;

	for (;;)
	{
		if (end_of_dirname == NULL)
		{
			if (next_path) *next_path = dirname_ptr;
			end_of_dirname = dirname_ptr;
			while (*end_of_dirname != 0 && *end_of_dirname != L'/' && *end_of_dirname != L'\\') end_of_dirname++;
			dirname_len = (size_t)(end_of_dirname - dirname_ptr);
			if (dirname_len == 0) return PhatState_InvalidParameter;
		}
		ret = Phat_NextDirItem(dir_info);
		if (ret == PhatState_OK)
		{
			if (dirname_len == dir_info->LFN_length &&
				!memcmp(dirname_ptr, dir_info->LFN_name, dirname_len * sizeof(WChar_t)))
			{
				Cluster_t dir_cluster;
				if (*end_of_dirname == 0)
				{
					// Point to the found item's SLN entry
					dir_info->cur_diritem--;
					return PhatState_OK;
				}
				if ((dir_info->attributes & ATTRIB_DIRECTORY) == 0) return PhatState_NotADirectory;
				end_of_dirname++;
				while (*end_of_dirname == L'/' || *end_of_dirname == L'\\')end_of_dirname++;
				dirname_ptr = end_of_dirname;
				dir_cluster = dir_info->first_cluster;
				dir_info->dir_start_cluster = dir_cluster;
				dir_info->dir_current_cluster = dir_cluster;
				dir_info->dir_current_cluster_index = 0;
				dir_info->cur_diritem = 0;
				end_of_dirname = NULL;
			}
		}
		else if (ret == PhatState_EndOfDirectory)
		{
			if (*end_of_dirname == 0) return ret;
			return PhatState_DirectoryNotFound;
		}
		else
		{
			return ret;
		}
	}
}

PhatBool_t Phat_IsValidFilename(WChar_p filename)
{
	// Check parameters
	if (!filename) return PhatState_InvalidParameter;

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

static PhatState Phat_FindShortFileName(Phat_DirInfo_p dir_info, const uint8_t *sfn83, PhatBool_p found)
{
	PhatState ret;

	*found = 0;
	dir_info->cur_diritem = 0;
	for (;;)
	{
		ret = Phat_NextDirItem(dir_info);
		if (ret == PhatState_EndOfDirectory)
		{
			return PhatState_OK;
		}
		else if (ret != PhatState_OK)
		{
			return ret;
		}
		if (!memcmp(dir_info->file_name_8_3, sfn83, 11))
		{
			*found = 1;
			return PhatState_OK;
		}
	}
}

static PhatState Phat_Gen83NameForLongFilename(Phat_DirInfo_p dir_info, const WChar_p longname, uint8_t *sfn83)
{
	PhatBool_t found = 0;
	PhatState ret;
	WChar_p ext;

	memset(sfn83, 0x20, 11);

	// Find extension name
	ext = Phat_ToEndOfString(longname);
	while (ext > longname)
	{
		ext--;
		if (*ext == L'.')
		{
			ext++;
			break;
		}
	}
	if (ext == longname)
	{
		// No extension name
		for (size_t i = 0; i < 8 && longname[i]; i++)
		{
			WChar_t ch = toupper(longname[i]);
			if (ch == '.') ch = '_';
			sfn83[i] = (uint8_t)ch;
		}
	}
	else
	{
		size_t end = ext - longname;
		if (end > 8) end = 8;
		for (size_t i = 0; i < end && longname[i]; i++)
		{
			WChar_t ch = toupper(longname[i]);
			if (ch == '.') ch = '_';
			sfn83[i] = (uint8_t)ch;
		}
		for (size_t i = 0; i < 3 && ext[i]; i++)
		{
			WChar_t ch = toupper(ext[i]);
			sfn83[8 + i] = (uint8_t)ch;
		}
	}
	if (sfn83[0] == ' ')
	{
		memcpy(sfn83, "NONAME", 6);
	}

	for (uint32_t index = 1;;)
	{
		ret = Phat_FindShortFileName(dir_info, sfn83, &found);
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

static PhatState Phat_CreateNewItemInDir(Phat_DirInfo_p dir_info, const WChar_p itemname, uint8_t attrib)
{
	PhatState ret = PhatState_OK;
	uint8_t name83[11];
	uint8_t case_info = 0;
	uint8_t items_needed;
	uint8_t free_count;
	Cluster_t first_diritem = 0;
	Cluster_t first_cluster = 0;
	size_t itemname_len;
	int only83 = 0;
	Phat_DirItem_t dir_item;
	Phat_p phat = dir_info->phat;

	itemname_len = Phat_Wcslen(itemname);
	if (itemname_len > MAX_LFN) return PhatState_NameTooLong;
	if (!Phat_IsValidFilename(itemname)) return PhatState_BadFileName;

	ret = Phat_FindItem(phat, itemname, dir_info, NULL);
	switch (ret)
	{
	case PhatState_OK:
		if (dir_info->attributes & ATTRIB_DIRECTORY)
			return PhatState_DirectoryAlreadyExists;
		else
			return PhatState_FileAlreadyExists;
	case PhatState_EndOfDirectory:
		break;
	default:
		return ret;
	}

	if (itemname_len <= 11 && Phat_IsFit83(itemname, name83, &case_info))
	{
		only83 = 1;
		items_needed = 1;
	}
	else
	{
		ret = Phat_Gen83NameForLongFilename(dir_info, itemname, name83);
		if (ret != PhatState_OK) return ret;
		items_needed = (uint8_t)(1 + ((itemname_len + 12) / 13));
	}

	dir_info->cur_diritem = 0;
	for (;;)
	{
		free_count = 0;
		ret = Phat_GetDirItem(dir_info, &dir_item);
		if (ret != PhatState_OK) return ret;
		if (dir_item.file_name_8_3[0] == 0x00 || dir_item.file_name_8_3[0] == 0xE5)
		{
			first_diritem = dir_info->cur_diritem;
			free_count = 1;
			ret = Phat_MoveToNextDirItemWithAllocation(dir_info);
			if (ret != PhatState_OK) return ret;
			while (free_count < items_needed)
			{
				ret = Phat_GetDirItem(dir_info, &dir_item);
				if (ret != PhatState_OK) return ret;
				if (dir_item.file_name_8_3[0] == 0x00 || dir_item.file_name_8_3[0] == 0xE5)
				{
					free_count++;
					if (free_count == items_needed)
						break;
					ret = Phat_MoveToNextDirItemWithAllocation(dir_info);
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
			ret = Phat_MoveToNextDirItemWithAllocation(dir_info);
			if (ret != PhatState_OK) return ret;
		}
		if (free_count == items_needed)
			break;
	}
	dir_info->cur_diritem = first_diritem;
	ret = Phat_UpdateClusterByDirItemIndex(dir_info, 1);
	if (ret != PhatState_OK) return ret;
	if (attrib & ATTRIB_DIRECTORY)
	{
		Phat_SectorCache_p cached_sector = NULL;
		Phat_DirItem_p dir_items;
		Cluster_t dir_start_cluster = dir_info->dir_start_cluster;
		if (dir_start_cluster == phat->root_dir_cluster) dir_start_cluster = 0;
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
		dir_items[1].first_cluster_high = dir_start_cluster >> 16;
		dir_items[1].last_modification_time = Phat_EncodeTime(&phat->cur_time);
		dir_items[1].last_modification_date = Phat_EncodeDate(&phat->cur_date);
		dir_items[1].first_cluster_low = dir_start_cluster & 0xFFFF;
		dir_items[1].file_size = 0;
		Phat_SetCachedSectorModified(cached_sector);
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
			if (itemname_len - offset < 13) copy_len = itemname_len - offset;
			memset(&lfn_entry, 0, sizeof lfn_entry);
			lfn_entry.order = i;
			if (i == lfn_entries_needed) lfn_entry.order |= 0x40;
			lfn_entry.attributes = ATTRIB_LFN;
			lfn_entry.type = 0;
			lfn_entry.first_cluster_low = 0;
			lfn_entry.checksum = checksum;
			for (size_t j = 0; j < 13; j++)
			{
				WChar_t ch = 0xFFFF;
				if (j < copy_len) ch = itemname[offset + j];
				if (j == copy_len) ch = 0;
				if (j < 5)
					lfn_entry.name1[j] = ch;
				else if (j < 11)
					lfn_entry.name2[j - 5] = ch;
				else
					lfn_entry.name3[j - 11] = ch;
			}
			ret = Phat_PutDirItem(dir_info, (Phat_DirItem_p)&lfn_entry);
			if (ret != PhatState_OK) return ret;
			ret = Phat_MoveToNextDirItem(dir_info);
			if (ret != PhatState_OK) return ret;
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
	ret = Phat_PutDirItem(dir_info, &dir_item);
	if (ret != PhatState_OK)
	{
		if (first_cluster > 2) Phat_UnlinkCluster(phat, first_cluster);
		return ret;
	}
	return PhatState_OK;
}

PhatState Phat_OpenFile(Phat_p phat, const WChar_p path, PhatBool_t readonly, Phat_FileInfo_p file_info)
{
	Phat_DirInfo_p dir_info;
	PhatState ret;
	WChar_p p;
	WChar_p ch;
	size_t dirname_len;

	// Check parameters
	if (!phat || !path || !file_info) return PhatState_InvalidParameter;

	if (!readonly && !phat->write_enable) return PhatState_ReadOnly;

	file_info->phat = phat;
	dir_info = &file_info->file_item;
	Phat_OpenRootDir(phat, dir_info);

	ret = Phat_FindItem(phat, path, dir_info, &p);
	if (ret == PhatState_EndOfDirectory)
	{
		if (readonly) return PhatState_FileNotFound;
		for (;;)
		{
			ch = p;
			while (*ch && *ch != L'/' && *ch != L'\\') ch++;
			dirname_len = (size_t)(ch - p);
			if (dirname_len > MAX_LFN) return PhatState_NameTooLong;
			if (*ch)
			{
				Phat_CloseDir(dir_info);
				return PhatState_DirectoryNotFound;
			}
			else
			{
				ret = Phat_MarkDirty(phat, 1, 1);
				if (ret != PhatState_OK) return ret;
				memcpy(phat->filename_buffer, p, dirname_len * sizeof(WChar_t));
				phat->filename_buffer[dirname_len] = 0;
				ret = Phat_CreateNewItemInDir(dir_info, phat->filename_buffer, 0);
				if (ret != PhatState_OK) return ret;
				ret = Phat_FindItem(phat, phat->filename_buffer, dir_info, NULL);
				if (ret != PhatState_OK) return ret;
				break;
			}
		}
	}
	else if (ret == PhatState_OK)
	{
		if (dir_info->attributes & ATTRIB_DIRECTORY)
		{
			Phat_CloseDir(dir_info);
			return PhatState_IsADirectory;
		}
	}
	else
	{
		// Error
		Phat_CloseDir(dir_info);
		return ret;
	}
	file_info->modified = 0;
	file_info->first_cluster = dir_info->first_cluster;
	file_info->cur_cluster = dir_info->first_cluster;
	file_info->file_size = dir_info->file_size;
	file_info->readonly = readonly || ((dir_info->attributes & ATTRIB_READ_ONLY) != 0);
	file_info->file_pointer = 0;
	file_info->cur_cluster_index = 0;
	file_info->sector_buffer_LBA = Phat_ClusterToLBA(phat, file_info->first_cluster) + phat->partition_start_LBA;
	if (file_info->file_size)
	{
		ret = Phat_ReadSectorsWithoutCache(phat, file_info->sector_buffer_LBA, 1, file_info->sector_buffer);
		if (ret != PhatState_OK) return ret;
	}
	return PhatState_OK;
}

static PhatState Phat_UpdateClusterByFilePointer(Phat_FileInfo_p file_info, PhatBool_t allocate_new_sectors)
{
	PhatState ret = PhatState_OK;
	Phat_p phat = file_info->phat;
	Cluster_t cluster_index;
	Cluster_t next_cluster;
	cluster_index = file_info->file_pointer / ((Cluster_t)phat->sectors_per_cluster * phat->bytes_per_sector);
	if (file_info->cur_cluster_index > cluster_index)
	{
		file_info->cur_cluster_index = 0;
		file_info->cur_cluster = file_info->first_cluster;
	}
	while (file_info->cur_cluster_index < cluster_index)
	{
		ret = Phat_GetFATNextCluster(phat, file_info->cur_cluster, &next_cluster);
		if (ret == PhatState_EndOfFATChain)
		{
			if (!allocate_new_sectors) return PhatState_EndOfFile;
			// Allocate a new cluster
			next_cluster = file_info->cur_cluster + 1;
			ret = Phat_AllocateCluster(phat, &next_cluster);
			if (ret != PhatState_OK) return ret;
			ret = Phat_WriteFAT(phat, file_info->cur_cluster, next_cluster, 0);
			if (ret != PhatState_OK) return ret;
			file_info->cur_cluster = next_cluster;
			ret = Phat_WipeCluster(phat, next_cluster);
			if (ret != PhatState_OK) return ret;
		}
		else if (ret != PhatState_OK)
			return ret;
		file_info->cur_cluster_index++;
		file_info->cur_cluster = next_cluster;
	}
	return PhatState_OK;
}

static PhatState Phat_GetCurFilePointerLBA(Phat_FileInfo_p file_info, LBA_p LBA_out, PhatBool_t allocate_new_sectors)
{
	uint8_t offset_in_cluster;
	PhatState ret = Phat_UpdateClusterByFilePointer(file_info, allocate_new_sectors);
	if (ret != PhatState_OK) return ret;
	offset_in_cluster = (file_info->file_pointer / file_info->phat->bytes_per_sector) % file_info->phat->sectors_per_cluster;
	*LBA_out = Phat_ClusterToLBA(file_info->phat, file_info->cur_cluster) + offset_in_cluster + file_info->phat->partition_start_LBA;
	return PhatState_OK;
}

PhatState Phat_ReadFile(Phat_FileInfo_p file_info, void *buffer, size_t bytes_to_read, size_t *bytes_read)
{
	PhatState ret = PhatState_OK;
	Phat_p phat = file_info->phat;
	uint16_t offset_in_sector;
	LBA_t FPLBA;
	size_t sectors_to_read;
	static size_t dummy;

	// Check parameters
	if (!file_info || !buffer || !bytes_to_read) return PhatState_InvalidParameter;
	if (!bytes_read) bytes_read = &dummy;
	*bytes_read = 0;
	if (file_info->first_cluster == 0) return PhatState_EndOfFile;
	if (file_info->file_pointer >= file_info->file_size) return PhatState_EndOfFile;
	if (file_info->file_pointer + bytes_to_read > file_info->file_size)
		bytes_to_read = file_info->file_size - file_info->file_pointer;
	offset_in_sector = file_info->file_pointer % 512;
	if (offset_in_sector)
	{
		size_t to_copy;
		ret = Phat_GetCurFilePointerLBA(file_info, &FPLBA, 0);
		if (ret != PhatState_OK) return ret;
		if (file_info->sector_buffer_LBA != FPLBA)
		{
			ret = Phat_ReadSectorsWithoutCache(phat, FPLBA, 1, file_info->sector_buffer);
			if (ret != PhatState_OK) return ret;
			file_info->sector_buffer_LBA = FPLBA;
		}
		to_copy = 512 - offset_in_sector;
		if (to_copy > bytes_to_read) to_copy = bytes_to_read;
		memcpy(buffer, &file_info->sector_buffer[offset_in_sector], to_copy);
		buffer = (uint8_t *)buffer + to_copy;
		bytes_to_read -= to_copy;
		file_info->file_pointer += (FileSize_t)to_copy;
		*bytes_read += to_copy;
	}
	sectors_to_read = bytes_to_read / 512;
	while (sectors_to_read)
	{
		ret = Phat_GetCurFilePointerLBA(file_info, &FPLBA, 0);
		if (ret != PhatState_OK) return ret;
		if (FPLBA == file_info->sector_buffer_LBA)
		{
			memcpy(buffer, file_info->sector_buffer, 512);
		}
		else
		{
			ret = Phat_ReadSectorsWithoutCache(phat, FPLBA, 1, buffer);
			if (ret != PhatState_OK) return ret;
		}
		sectors_to_read--;
		buffer = (uint8_t *)buffer + 512;
		file_info->file_pointer += 512;
		*bytes_read += 512;
		bytes_to_read -= 512;
	}
	if (bytes_to_read)
	{
		ret = Phat_GetCurFilePointerLBA(file_info, &FPLBA, 0);
		if (ret != PhatState_OK) return ret;
		if (file_info->sector_buffer_LBA != FPLBA)
		{
			ret = Phat_ReadSectorsWithoutCache(phat, FPLBA, 1, file_info->sector_buffer);
			if (ret != PhatState_OK) return ret;
			file_info->sector_buffer_LBA = FPLBA;
		}
		ret = Phat_ReadSectorsWithoutCache(phat, FPLBA, 1, file_info->sector_buffer);
		if (ret != PhatState_OK) return ret;
		file_info->sector_buffer_LBA = FPLBA;
		memcpy(buffer, file_info->sector_buffer, bytes_to_read);
		file_info->file_pointer += (FileSize_t)bytes_to_read;
		*bytes_read += bytes_to_read;
	}
	return file_info->file_pointer >= file_info->file_size ? PhatState_EndOfFile : PhatState_OK;
}

PhatState Phat_WriteFile(Phat_FileInfo_p file_info, const void *buffer, size_t bytes_to_write, size_t *bytes_written)
{
	PhatState ret = PhatState_OK;
	Phat_p phat = file_info->phat;
	uint16_t offset_in_sector;
	LBA_t FPLBA;
	size_t sectors_to_write;
	static size_t dummy;
	Phat_DirInfo_p dir_info = &file_info->file_item;

	// Check parameters
	if (!file_info || !buffer || !bytes_to_write) return PhatState_InvalidParameter;
	if (file_info->readonly || !phat->write_enable) return PhatState_ReadOnly;
	if (file_info->file_item.attributes & ATTRIB_READ_ONLY) return PhatState_ReadOnly;
	if (!bytes_written) bytes_written = &dummy;
	*bytes_written = 0;
	offset_in_sector = file_info->file_pointer % 512;
	if (file_info->first_cluster == 0)
	{
		// Allocate cluster for empty file
		Cluster_t new_cluster = 0;
		Phat_DirItem_t dir_item;
		ret = Phat_AllocateCluster(phat, &new_cluster);
		if (ret != PhatState_OK) return ret;
		ret = Phat_GetDirItem(dir_info, &dir_item);
		if (ret != PhatState_OK) return ret;
		dir_item.first_cluster_low = new_cluster & 0xFFFF;
		dir_item.first_cluster_high = new_cluster >> 16;
		ret = Phat_PutDirItem(dir_info, &dir_item);
		if (ret != PhatState_OK) return ret;
		file_info->first_cluster = new_cluster;
		file_info->cur_cluster = new_cluster;
	}
	if (offset_in_sector)
	{
		size_t to_copy;
		ret = Phat_GetCurFilePointerLBA(file_info, &FPLBA, 1);
		if (ret != PhatState_OK) return ret;
		if (file_info->sector_buffer_LBA != FPLBA)
		{
			ret = Phat_ReadSectorsWithoutCache(phat, FPLBA, 1, file_info->sector_buffer);
			if (ret != PhatState_OK) return ret;
			file_info->sector_buffer_LBA = FPLBA;
		}
		to_copy = 512 - offset_in_sector;
		if (to_copy > bytes_to_write) to_copy = bytes_to_write;
		memcpy(&file_info->sector_buffer[offset_in_sector], buffer, to_copy);
		ret = Phat_WriteSectorsWithoutCache(phat, FPLBA, 1, file_info->sector_buffer);
		if (ret != PhatState_OK) return ret;
		file_info->modified = 1;
		buffer = (uint8_t *)buffer + to_copy;
		bytes_to_write -= to_copy;
		file_info->file_pointer += (FileSize_t)to_copy;
		*bytes_written += to_copy;
	}
	sectors_to_write = bytes_to_write / 512;
	while (sectors_to_write)
	{
		ret = Phat_GetCurFilePointerLBA(file_info, &FPLBA, 1);
		if (ret != PhatState_OK) return ret;
		if (FPLBA == file_info->sector_buffer_LBA)
		{
			memcpy(file_info->sector_buffer, buffer, 512);
		}
		ret = Phat_WriteSectorsWithoutCache(phat, FPLBA, 1, buffer);
		if (ret != PhatState_OK) return ret;
		file_info->modified = 1;
		sectors_to_write--;
		buffer = (uint8_t *)buffer + 512;
		file_info->file_pointer += 512;
		*bytes_written += 512;
		bytes_to_write -= 512;
	}
	if (bytes_to_write)
	{
		ret = Phat_GetCurFilePointerLBA(file_info, &FPLBA, 1);
		if (ret != PhatState_OK) return ret;
		memset(file_info->sector_buffer, 0, sizeof file_info->sector_buffer);
		memcpy(file_info->sector_buffer, buffer, bytes_to_write);
		file_info->sector_buffer_LBA = FPLBA;
		ret = Phat_WriteSectorsWithoutCache(phat, FPLBA, 1, file_info->sector_buffer);
		if (ret != PhatState_OK) return ret;
		file_info->modified = 1;
		file_info->file_pointer += (FileSize_t)bytes_to_write;
		*bytes_written += bytes_to_write;
	}
	if (file_info->file_pointer > file_info->file_size) file_info->file_size = file_info->file_pointer;
	return PhatState_OK;
}

PhatState Phat_SeekFile(Phat_FileInfo_p file_info, FileSize_t position)
{
	// Check parameters
	if (!file_info) return PhatState_InvalidParameter;

	file_info->file_pointer = position;
	if (Phat_IsEOF(file_info)) return PhatState_EndOfFile;
	else return PhatState_OK;
}

void Phat_GetFilePointer(Phat_FileInfo_p file_info, FileSize_t *position)
{
	*position = file_info->file_pointer;
}

void Phat_GetFileSize(Phat_FileInfo_p file_info, FileSize_t *size)
{
	*size = file_info->file_size;
}

PhatBool_t Phat_IsEOF(Phat_FileInfo_p file_info)
{
	return file_info->file_pointer >= file_info->file_size;
}

PhatState Phat_CloseFile(Phat_FileInfo_p file_info)
{
	Phat_DirItem_t diritem;
	PhatState ret;
	Phat_p phat = file_info->phat;
	Phat_DirInfo_p dir_info = &file_info->file_item;

	// Check parameters
	if (!file_info) return PhatState_InvalidParameter;

	if (phat->write_enable)
	{
		ret = Phat_GetDirItem(dir_info, &diritem);
		if (ret != PhatState_OK) return ret;

		diritem.last_access_date = Phat_EncodeDate(&phat->cur_date);
		if (file_info->modified)
		{
			diritem.last_modification_date = Phat_EncodeDate(&phat->cur_date);
			diritem.last_modification_time = Phat_EncodeTime(&phat->cur_time);
			diritem.file_size = file_info->file_size;
		}
		ret = Phat_PutDirItem(dir_info, &diritem);
		if (ret != PhatState_OK) return ret;
	}

	Phat_CloseDir(dir_info);
	memset(file_info, 0, sizeof * file_info);
	return PhatState_OK;
}

PhatState Phat_CreateDirectory(Phat_p phat, const WChar_p path)
{
	Phat_DirInfo_t dir_info;
	PhatState ret;
	WChar_p p = path;
	WChar_p ch;
	size_t dirname_len;

	// Check parameters
	if (!phat || !path) return PhatState_InvalidParameter;
	if (!phat->write_enable) return PhatState_ReadOnly;

	Phat_OpenRootDir(phat, &dir_info);

	ret = Phat_FindItem(phat, p, &dir_info, &p);
	switch (ret)
	{
	case PhatState_OK:
		if (dir_info.attributes & ATTRIB_DIRECTORY)
			return PhatState_DirectoryAlreadyExists;
		else
			return PhatState_FileAlreadyExists;
		return PhatState_NotADirectory;
	case PhatState_EndOfDirectory:
	case PhatState_DirectoryNotFound:
		break;
	default:
		return ret;
	}
	for (;;)
	{
		ch = p;
		while (*ch && *ch != L'/' && *ch != L'\\') ch++;
		dirname_len = (size_t)(ch - p);
		if (dirname_len > MAX_LFN) return PhatState_NameTooLong;
		memcpy(phat->filename_buffer, p, dirname_len * sizeof(WChar_t));
		phat->filename_buffer[dirname_len] = 0;
		ret = Phat_CreateNewItemInDir(&dir_info, phat->filename_buffer, ATTRIB_DIRECTORY);
		if (ret != PhatState_OK) return ret;
		ret = Phat_ChDir(&dir_info, phat->filename_buffer);
		if (ret != PhatState_OK) return ret;
		if (!*ch) return PhatState_OK;
		while (*ch == L'/' || *ch == L'\\') ch++;
		p = ch;
	}
}

PhatState Phat_RemoveDirectory(Phat_p phat, const WChar_p path)
{
	Phat_DirInfo_t dir_info;
	PhatState ret;
	size_t name_len;
	Cluster_t dir_start_cluster;
	Cluster_t parent_dir_start_cluster = 0;
	Cluster_t first_entry = 0;
	Cluster_t last_entry = 0;

	// Check parameters
	if (!phat || !path) return PhatState_InvalidParameter;
	if (!phat->write_enable) return PhatState_ReadOnly;

	ret = Phat_OpenDir(phat, path, &dir_info);
	if (ret != PhatState_OK) return ret;
	dir_start_cluster = dir_info.dir_start_cluster;
	for (;;)
	{
		ret = Phat_NextDirItem(&dir_info);
		if (ret == PhatState_OK)
		{
			if (!Phat_Wcscmp(dir_info.LFN_name, L".")) continue;
			if (!Phat_Wcscmp(dir_info.LFN_name, L".."))
			{
				parent_dir_start_cluster = dir_info.first_cluster;
				continue;
			}
			return PhatState_DirectoryNotEmpty;
		}
		if (ret == PhatState_EndOfDirectory) break;
		else goto FailExit;
	}
	Phat_PathToName(path, phat->filename_buffer);
	name_len = Phat_Wcslen(phat->filename_buffer);

	if (parent_dir_start_cluster == 0) parent_dir_start_cluster = phat->root_dir_cluster;

	// Get to ".."
	dir_info.dir_start_cluster = parent_dir_start_cluster;
	dir_info.dir_current_cluster = parent_dir_start_cluster;
	dir_info.dir_current_cluster_index = 0;
	dir_info.cur_diritem = 0;

	ret = Phat_FindItem(phat, phat->filename_buffer, &dir_info, NULL);
	if (ret != PhatState_OK) goto FailExit;
	last_entry = dir_info.cur_diritem;
	ret = Phat_FindFirstLFNEntry(&dir_info);
	if (ret != PhatState_OK) goto FailExit;
	first_entry = dir_info.cur_diritem;

	for (Cluster_t i = first_entry; i <= last_entry; i++)
	{
		Phat_DirItem_t dir_item;
		dir_info.cur_diritem = i;
		ret = Phat_GetDirItem(&dir_info, &dir_item);
		if (ret != PhatState_OK) return ret;
		dir_item.file_name_8_3[0] = 0xE5;
		ret = Phat_PutDirItem(&dir_info, &dir_item);
		if (ret != PhatState_OK) return ret;
	}

	// Mark the clusters of the directory as free clusters.
	ret = Phat_UnlinkCluster(phat, dir_start_cluster);
	if (ret != PhatState_OK) goto FailExit;

FailExit:
	Phat_CloseDir(&dir_info);
	return ret;
}

PhatState Phat_DeleteFile(Phat_p phat, const WChar_p path)
{
	PhatState ret;
	Phat_DirInfo_t dir_info;
	Cluster_t first_entry = 0;
	Cluster_t last_entry = 0;
	Cluster_t first_cluster;

	// Check parameters
	if (!phat || !path) return PhatState_InvalidParameter;
	if (!phat->write_enable) return PhatState_ReadOnly;

	Phat_OpenRootDir(phat, &dir_info);
	ret = Phat_FindItem(phat, path, &dir_info, NULL);
	if (ret != PhatState_OK) return ret;

	first_cluster = dir_info.first_cluster;

	last_entry = dir_info.cur_diritem;
	ret = Phat_FindFirstLFNEntry(&dir_info);
	if (ret != PhatState_OK) goto FailExit;
	first_entry = dir_info.cur_diritem;

	for (Cluster_t i = first_entry; i <= last_entry; i++)
	{
		Phat_DirItem_t dir_item;
		dir_info.cur_diritem = i;
		ret = Phat_GetDirItem(&dir_info, &dir_item);
		if (ret != PhatState_OK) return ret;
		dir_item.file_name_8_3[0] = 0xE5;
		ret = Phat_PutDirItem(&dir_info, &dir_item);
		if (ret != PhatState_OK) return ret;
	}

	// Mark the clusters of the file as free clusters.
	ret = Phat_UnlinkCluster(phat, first_cluster);
	if (ret != PhatState_OK) goto FailExit;

FailExit:
	Phat_CloseDir(&dir_info);
	return ret;
}

PhatState Phat_Rename(Phat_p phat, const WChar_p path, const WChar_p new_name)
{
	PhatState ret;
	Phat_DirInfo_t dir_info;
	Phat_DirItem_t dir_item;
	Cluster_t first_entry;
	Cluster_t last_entry;
	Cluster_t first_cluster;
	Cluster_t file_size;
	uint16_t cdate;
	uint16_t ctime;
	uint16_t mdate;
	uint16_t mtime;
	uint16_t adate;
	uint8_t ctime_tenths;
	uint8_t attributes;
	uint8_t case_info;

	// Check parameters
	if (!phat || !path || !new_name) return PhatState_InvalidParameter;
	if (!phat->write_enable) return PhatState_ReadOnly;

	// Ensure the new name is valid
	if (!Phat_IsValidFilename(new_name)) return PhatState_BadFileName;

	// If the new name equals to the old name, return OK
	if (!Phat_Wcscmp(new_name, dir_info.LFN_name)) return PhatState_OK;

	// Find the file/dir we want to rename
	Phat_OpenRootDir(phat, &dir_info);
	ret = Phat_FindItem(phat, path, &dir_info, NULL);
	if (ret != PhatState_OK) return ret;

	// Check if the current directory contains the file that's name is same as the new name
	ret = Phat_FindItem(phat, new_name, &dir_info, NULL);
	if (ret == PhatState_OK)
	{
		// Find item is successful, the item is found, so can't move the file over there.
		if (dir_info.attributes & ATTRIB_DIRECTORY)
			return PhatState_DirectoryAlreadyExists;
		else
			return PhatState_FileAlreadyExists;
	}
	else if (ret != PhatState_EndOfDirectory) return ret;

	// Find the file/dir we want to rename
	Phat_OpenRootDir(phat, &dir_info);
	ret = Phat_FindItem(phat, path, &dir_info, NULL);
	if (ret != PhatState_OK) return ret;

	// Get the file/dir informations
	ret = Phat_GetDirItem(&dir_info, &dir_item);
	if (ret != PhatState_OK) return ret;

	first_cluster = dir_info.first_cluster;
	file_size = dir_info.file_size;
	cdate = dir_item.creation_date;
	ctime = dir_item.creation_time;
	mdate = dir_item.last_modification_date;
	mtime = dir_item.last_modification_time;
	adate = dir_item.last_access_date;
	ctime_tenths = dir_item.creation_time_tenths;
	attributes = dir_item.attributes;
	case_info = dir_item.case_info;

	last_entry = dir_info.cur_diritem;
	ret = Phat_FindFirstLFNEntry(&dir_info);
	if (ret != PhatState_OK) return ret;
	first_entry = dir_info.cur_diritem;

	// Remove the old entries
	for (Cluster_t i = first_entry; i <= last_entry; i++)
	{
		dir_info.cur_diritem = i;
		ret = Phat_GetDirItem(&dir_info, &dir_item);
		if (ret != PhatState_OK) return ret;
		dir_item.file_name_8_3[0] = 0xE5;
		ret = Phat_PutDirItem(&dir_info, &dir_item);
		if (ret != PhatState_OK) return ret;
	}

	// Create the new entries
	ret = Phat_CreateNewItemInDir(&dir_info, new_name, 0);
	if (ret != PhatState_OK) return ret;

	// Move the file/dir info into the new SFN entry
	ret = Phat_GetDirItem(&dir_info, &dir_item);
	if (ret != PhatState_OK) return ret;

	dir_item.first_cluster_low = first_cluster & 0xFFFF;
	dir_item.first_cluster_high = first_cluster >> 16;
	dir_item.file_size = file_size;
	dir_item.creation_date = cdate;
	dir_item.creation_time = ctime;
	dir_item.last_modification_date = mdate;
	dir_item.last_modification_time = mtime;
	dir_item.last_access_date = adate;
	dir_item.attributes = attributes;
	dir_item.case_info = case_info;

	ret = Phat_PutDirItem(&dir_info, &dir_item);
	if (ret != PhatState_OK) return ret;

	return PhatState_OK;
}

PhatState Phat_Move(Phat_p phat, const WChar_p oldpath, const WChar_p newpath)
{
	PhatState ret;
	Phat_DirInfo_t dir_info1;
	Phat_DirInfo_t dir_info2;
	Phat_DirItem_t dir_item;
	Cluster_t first_entry;
	Cluster_t last_entry;
	Cluster_t first_cluster;
	Cluster_t file_size;
	uint16_t cdate;
	uint16_t ctime;
	uint16_t mdate;
	uint16_t mtime;
	uint16_t adate;
	uint8_t ctime_tenths;
	uint8_t attributes;
	uint8_t case_info;

	// Check parameters
	if (!phat || !oldpath || !newpath) return PhatState_InvalidParameter;
	if (!phat->write_enable) return PhatState_ReadOnly;

	// Ensure the target directory is valid
	ret = Phat_OpenDir(phat, newpath, &dir_info2);
	if (ret != PhatState_OK) return ret;

	// Get to the parent directory of the object to be moved
	Phat_OpenRootDir(phat, &dir_info1);
	ret = Phat_FindItem(phat, oldpath, &dir_info1, NULL);
	if (ret != PhatState_OK) return ret;

	// If the old path is the root directory, the move action is not allowed.
	if (dir_info1.first_cluster == 0 || dir_info1.first_cluster == 2) return PhatState_InvalidParameter;

	// Check if the new directory is the sub directory of the old path
	// If so, the move will cause file system error so this must not be allowed.
	for (;;)
	{
		if (dir_info2.dir_start_cluster == 0 || dir_info2.dir_start_cluster == 2) break;
		ret = Phat_ChDir(&dir_info2, L"..");
		if (ret == PhatState_DirectoryNotFound)
		{
			// A non root directory doesn't have a ".." error, this is a file system error
			return PhatState_FSError;
		}
		if (ret != PhatState_OK) return ret;
		if (dir_info2.dir_start_cluster == dir_info1.first_cluster)
		{
			return PhatState_InvalidParameter;
		}
	}
	ret = Phat_OpenDir(phat, newpath, &dir_info2);
	if (ret != PhatState_OK) return ret;

	// Check if the target directory contains the same name file
	Phat_Wcscpy(phat->filename_buffer, dir_info1.LFN_name);
	ret = Phat_FindItem(phat, phat->filename_buffer, &dir_info2, NULL);
	if (ret == PhatState_OK)
	{
		// Find item is successful, the item is found, so can't move the file over there.
		if (dir_info2.attributes & ATTRIB_DIRECTORY)
			return PhatState_DirectoryAlreadyExists;
		else
			return PhatState_FileAlreadyExists;
	}
	else if (ret != PhatState_EndOfDirectory) return ret;

	// Get the file/dir informations
	ret = Phat_GetDirItem(&dir_info1, &dir_item);
	if (ret != PhatState_OK) return ret;

	first_cluster = dir_info1.first_cluster;
	file_size = dir_info1.file_size;
	cdate = dir_item.creation_date;
	ctime = dir_item.creation_time;
	mdate = dir_item.last_modification_date;
	mtime = dir_item.last_modification_time;
	adate = dir_item.last_access_date;
	ctime_tenths = dir_item.creation_time_tenths;
	attributes = dir_item.attributes;
	case_info = dir_item.case_info;

	last_entry = dir_info1.cur_diritem;
	ret = Phat_FindFirstLFNEntry(&dir_info1);
	if (ret != PhatState_OK) return ret;
	first_entry = dir_info1.cur_diritem;

	// Create a file/dir in the target directory
	ret = Phat_CreateNewItemInDir(&dir_info2, phat->filename_buffer, 0);
	if (ret != PhatState_OK) return ret;

	// Move the file/dir info into the new SFN entry
	ret = Phat_GetDirItem(&dir_info2, &dir_item);
	if (ret != PhatState_OK) return ret;

	dir_item.first_cluster_low = first_cluster & 0xFFFF;
	dir_item.first_cluster_high = first_cluster >> 16;
	dir_item.file_size = file_size;
	dir_item.creation_date = cdate;
	dir_item.creation_time = ctime;
	dir_item.last_modification_date = mdate;
	dir_item.last_modification_time = mtime;
	dir_item.last_access_date = adate;
	dir_item.attributes = attributes;
	dir_item.case_info = case_info;

	ret = Phat_PutDirItem(&dir_info2, &dir_item);
	if (ret != PhatState_OK) return ret;

	// Remove the old path file entries
	for (Cluster_t i = first_entry; i <= last_entry; i++)
	{
		dir_info1.cur_diritem = i;
		ret = Phat_GetDirItem(&dir_info1, &dir_item);
		if (ret != PhatState_OK) return ret;
		dir_item.file_name_8_3[0] = 0xE5;
		ret = Phat_PutDirItem(&dir_info1, &dir_item);
		if (ret != PhatState_OK) return ret;
	}
	return PhatState_OK;
}

PhatState Phat_MakeFS_And_Mount(Phat_p phat, int partition_index, LBA_t partition_start_LBA, LBA_t partition_size_in_sectors, int FAT_bits, uint16_t root_dir_entry_count, uint32_t volume_ID, const char *volume_lable)
{
	uint8_t num_FATs;
	uint8_t media_type;
	uint8_t sectors_per_cluster;
	uint16_t reserved_sector_count;
	uint16_t sectors_per_track;
	uint16_t num_heads;
	uint16_t BIOS_drive_number;
	uint32_t FAT_size;
	Cluster_t max_cluster;
	Cluster_t partition_size_in_clusters;
	Cluster_t free_clusters;
	LBA_t end_of_FAT_LBA;
	PhatState ret;
	Phat_SectorCache_p cached_sector;
	const uint8_t *boot_code;

	if (partition_size_in_sectors >= 0xFFFFFFFF / 512) return PhatState_CannotMakeFS;
	if (partition_start_LBA == 0 && partition_index != 0) return PhatState_InvalidParameter;

	num_FATs = 2;
	if (!volume_lable) volume_lable = "NO NAME";

	switch (FAT_bits)
	{
	case 12:
		max_cluster = 0xFF0;
		if (!root_dir_entry_count) root_dir_entry_count = 224;
		media_type = 0xF0;
		BIOS_drive_number = 0x00;
		break;
	case 16:
		max_cluster = 0xFFF0;
		if (!root_dir_entry_count) root_dir_entry_count = 512;
		media_type = 0xF8;
		BIOS_drive_number = 0x80;
		break;
	case 32:
		if (root_dir_entry_count) return PhatState_InvalidParameter;
		max_cluster = 0x0FFFFFF0;
		media_type = 0xF8;
		BIOS_drive_number = 0x80;
		break;
	case 0:
		if (partition_size_in_sectors >= 8 * 0xFFF0)
		{
			FAT_bits = 32;
			max_cluster = 0x0FFFFFF0;
			if (root_dir_entry_count) return PhatState_InvalidParameter;
		}
		else if (partition_size_in_sectors >= 8 * 0xFF0)
		{
			FAT_bits = 16;
			max_cluster = 0xFFF0;
			if (!root_dir_entry_count) root_dir_entry_count = 512;
		}
		else
		{
			FAT_bits = 12;
			max_cluster = 0xFF0;
			if (!root_dir_entry_count) root_dir_entry_count = 224;
		}
		break;
	default:
		return PhatState_InvalidParameter;
	}

	if (FAT_bits == 12 && partition_start_LBA == 0 && partition_size_in_sectors == 2880)
	{
		sectors_per_track = 0x0012;
		num_heads = 0x02;
	}
	else
	{
		sectors_per_track = 0x003F;
		num_heads = 0xFF;
	}

	// Choose the smallest possible `sectors_per_cluster` that still covers the entire partition.
	if (partition_size_in_sectors >= (uint64_t)128 * max_cluster) return PhatState_CannotMakeFS;
	else if (partition_size_in_sectors > (uint64_t)64 * max_cluster) sectors_per_cluster = 128;
	else if (partition_size_in_sectors > (uint64_t)32 * max_cluster) sectors_per_cluster = 64;
	else if (partition_size_in_sectors > 16 * max_cluster) sectors_per_cluster = 32;
	else if (partition_size_in_sectors > 8 * max_cluster) sectors_per_cluster = 16;
	else if (partition_size_in_sectors > 4 * max_cluster) sectors_per_cluster = 8;
	else if (partition_size_in_sectors > 2 * max_cluster) sectors_per_cluster = 4;
	else if (partition_size_in_sectors > 1 * max_cluster) sectors_per_cluster = 2;
	else sectors_per_cluster = 1;

	partition_size_in_clusters = partition_size_in_sectors / sectors_per_cluster;
	partition_size_in_sectors = partition_size_in_clusters * sectors_per_cluster;

	switch (FAT_bits)
	{
	case 12: FAT_size = (partition_size_in_clusters + (partition_size_in_clusters >> 1)) / 512; break;
	case 16: FAT_size = (partition_size_in_clusters * 2) / 512; break;
	case 32: FAT_size = (partition_size_in_clusters * 4) / 512; break;
	}

	if (sectors_per_cluster > 1) FAT_size = ((FAT_size - 1) / sectors_per_cluster + 1) * sectors_per_cluster;
	reserved_sector_count = 32;

	free_clusters = (partition_size_in_sectors - reserved_sector_count - FAT_size * num_FATs) / sectors_per_cluster - 2 - 1;

	phat->partition_start_LBA = partition_start_LBA;
	phat->FAT_size_in_sectors = FAT_size;
	end_of_FAT_LBA = reserved_sector_count + num_FATs * FAT_size;
	phat->total_sectors = partition_size_in_sectors;
	phat->num_FATs = num_FATs;
	phat->FAT1_start_LBA = reserved_sector_count;
	phat->root_dir_start_LBA = end_of_FAT_LBA;
	phat->root_dir_entry_count = root_dir_entry_count;
	phat->bytes_per_sector = 512;
	phat->sectors_per_cluster = sectors_per_cluster;
	phat->num_diritems_in_a_sector = 512 / 32;
	phat->num_diritems_in_a_cluster = (phat->bytes_per_sector * phat->sectors_per_cluster) / 32;
	phat->num_FAT_entries = (phat->FAT_size_in_sectors * phat->bytes_per_sector * 8) / phat->FAT_bits;
	phat->FATs_are_same = 1;
	phat->max_valid_cluster = phat->num_FAT_entries + 1;
	phat->free_clusters = free_clusters;
	phat->next_free_cluster = 3;
	phat->is_dirty = 0;

	ret = Phat_ReadSectorThroughCache(phat, partition_start_LBA, &cached_sector);
	if (ret != PhatState_OK) return ret;

	if (FAT_bits != 32)
	{
		uint16_t num_root_dir_sectors = ((uint32_t)root_dir_entry_count * 32 + 511) / 512;
		Phat_DBR_FAT_p dbr = (Phat_DBR_FAT_p)&cached_sector->data;
		dbr->jump_boot[0] = 0xEB;
		dbr->jump_boot[1] = 0x3C;
		dbr->jump_boot[2] = 0x90;
		memcpy(dbr->OEM_name, "*-v4VIHC", 8);
		dbr->bytes_per_sector = 512;
		dbr->sectors_per_cluster = sectors_per_cluster;
		dbr->reserved_sector_count = reserved_sector_count;
		dbr->num_FATs = num_FATs;
		dbr->root_dir_entry_count = root_dir_entry_count;
		dbr->total_sectors_16 = partition_size_in_sectors <= 0xFFFF ? partition_size_in_sectors : 0;
		dbr->media = media_type;
		dbr->FAT_size = FAT_size;
		dbr->sectors_per_track = sectors_per_track;
		dbr->num_heads = num_heads;
		dbr->hidden_sectors = (uint32_t)partition_start_LBA;
		dbr->total_sectors_32 = partition_size_in_sectors > 0xFFFF ? partition_size_in_sectors : 0;
		dbr->BIOS_drive_number = (uint8_t)BIOS_drive_number;
		dbr->first_head = 0x00;
		dbr->extension_flag = 0x29;
		dbr->volume_ID = volume_ID;
		memset(dbr->volume_label, 0x20, sizeof dbr->volume_label);
		memcpy(dbr->volume_label, volume_lable, strlen(volume_lable));
		switch (FAT_bits)
		{
		case 12:
			memcpy(dbr->file_system_type, "FAT12   ", 8);
			boot_code =
				"\x33\xC9\x8E\xD1\xBC\xFC\x7B\x16\x07\xBD\x78\x00\xC5\x76\x00\x1E"
				"\x56\x16\x55\xBF\x22\x05\x89\x7E\x00\x89\x4E\x02\xB1\x0B\xFC\xF3"
				"\xA4\x06\x1F\xBD\x00\x7C\xC6\x45\xFE\x0F\x38\x4E\x24\x7D\x20\x8B"
				"\xC1\x99\xE8\x7E\x01\x83\xEB\x3A\x66\xA1\x1C\x7C\x66\x3B\x07\x8A"
				"\x57\xFC\x75\x06\x80\xCA\x02\x88\x56\x02\x80\xC3\x10\x73\xED\x33"
				"\xC9\xFE\x06\xD8\x7D\x8A\x46\x10\x98\xF7\x66\x16\x03\x46\x1C\x13"
				"\x56\x1E\x03\x46\x0E\x13\xD1\x8B\x76\x11\x60\x89\x46\xFC\x89\x56"
				"\xFE\xB8\x20\x00\xF7\xE6\x8B\x5E\x0B\x03\xC3\x48\xF7\xF3\x01\x46"
				"\xFC\x11\x4E\xFE\x61\xBF\x00\x07\xE8\x28\x01\x72\x3E\x38\x2D\x74"
				"\x17\x60\xB1\x0B\xBE\xD8\x7D\xF3\xA6\x61\x74\x3D\x4E\x74\x09\x83"
				"\xC7\x20\x3B\xFB\x72\xE7\xEB\xDD\xFE\x0E\xD8\x7D\x7B\xA7\xBE\x7F"
				"\x7D\xAC\x98\x03\xF0\xAC\x98\x40\x74\x0C\x48\x74\x13\xB4\x0E\xBB"
				"\x07\x00\xCD\x10\xEB\xEF\xBE\x82\x7D\xEB\xE6\xBE\x80\x7D\xEB\xE1"
				"\xCD\x16\x5E\x1F\x66\x8F\x04\xCD\x19\xBE\x81\x7D\x8B\x7D\x1A\x8D"
				"\x45\xFE\x8A\x4E\x0D\xF7\xE1\x03\x46\xFC\x13\x56\xFE\xB1\x04\xE8"
				"\xC2\x00\x72\xD7\xEA\x00\x02\x70\x00\x52\x50\x06\x53\x6A\x01\x6A"
				"\x10\x91\x8B\x46\x18\xA2\x26\x05\x96\x92\x33\xD2\xF7\xF6\x91\xF7"
				"\xF6\x42\x87\xCA\xF7\x76\x1A\x8A\xF2\x8A\xE8\xC0\xCC\x02\x0A\xCC"
				"\xB8\x01\x02\x80\x7E\x02\x0E\x75\x04\xB4\x42\x8B\xF4\x8A\x56\x24"
				"\xCD\x13\x61\x61\x72\x0A\x40\x75\x01\x42\x03\x5E\x0B\x49\x75\x77"
				"\xC3\x03\x18\x01\x27\x0D\x0A\x49\x6E\x76\x61\x6C\x69\x64\x20\x73"
				"\x79\x73\x74\x65\x6D\x20\x64\x69\x73\x6B\xFF\x0D\x0A\x44\x69\x73"
				"\x6B\x20\x49\x2F\x4F\x20\x65\x72\x72\x6F\x72\xFF\x0D\x0A\x52\x65"
				"\x70\x6C\x61\x63\x65\x20\x74\x68\x65\x20\x64\x69\x73\x6B\x2C\x20"
				"\x61\x6E\x64\x20\x74\x68\x65\x6E\x20\x70\x72\x65\x73\x73\x20\x61"
				"\x6E\x79\x20\x6B\x65\x79\x0D\x0A\x00\x00\x49\x4F\x20\x20\x20\x20"
				"\x20\x20\x53\x59\x53\x4D\x53\x44\x4F\x53\x20\x20\x20\x53\x59\x53"
				"\x7F\x01\x00\x41\xBB\x00\x07\x60\x66\x6A\x00\xE9\x3B\xFF\x00\x00";
			phat->end_of_cluster_chain = 0xFF8;
			break;
		case 16:
			memcpy(dbr->file_system_type, "FAT16   ", 8);
			boot_code =
				"\x33\xC9\x8E\xD1\xBC\xF0\x7B\x8E\xD9\xB8\x00\x20\x8E\xC0\xFC\xBD"
				"\x00\x7C\x38\x4E\x24\x7D\x24\x8B\xC1\x99\xE8\x3C\x01\x72\x1C\x83"
				"\xEB\x3A\x66\xA1\x1C\x7C\x26\x66\x3B\x07\x26\x8A\x57\xFC\x75\x06"
				"\x80\xCA\x02\x88\x56\x02\x80\xC3\x10\x73\xEB\x33\xC9\x8A\x46\x10"
				"\x98\xF7\x66\x16\x03\x46\x1C\x13\x56\x1E\x03\x46\x0E\x13\xD1\x8B"
				"\x76\x11\x60\x89\x46\xFC\x89\x56\xFE\xB8\x20\x00\xF7\xE6\x8B\x5E"
				"\x0B\x03\xC3\x48\xF7\xF3\x01\x46\xFC\x11\x4E\xFE\x61\xBF\x00\x00"
				"\xE8\xE6\x00\x72\x39\x26\x38\x2D\x74\x17\x60\xB1\x0B\xBE\xA1\x7D"
				"\xF3\xA6\x61\x74\x32\x4E\x74\x09\x83\xC7\x20\x3B\xFB\x72\xE6\xEB"
				"\xDC\xA0\xFB\x7D\xB4\x7D\x8B\xF0\xAC\x98\x40\x74\x0C\x48\x74\x13"
				"\xB4\x0E\xBB\x07\x00\xCD\x10\xEB\xEF\xA0\xFD\x7D\xEB\xE6\xA0\xFC"
				"\x7D\xEB\xE1\xCD\x16\xCD\x19\x26\x8B\x55\x1A\x52\xB0\x01\xBB\x00"
				"\x00\xE8\x3B\x00\x72\xE8\x5B\x8A\x56\x24\xBE\x0B\x7C\x8B\xFC\xC7"
				"\x46\xF0\x3D\x7D\xC7\x46\xF4\x29\x7D\x8C\xD9\x89\x4E\xF2\x89\x4E"
				"\xF6\xC6\x06\x96\x7D\xCB\xEA\x03\x00\x00\x20\x0F\xB6\xC8\x66\x8B"
				"\x46\xF8\x66\x03\x46\x1C\x66\x8B\xD0\x66\xC1\xEA\x10\xEB\x5E\x0F"
				"\xB6\xC8\x4A\x4A\x8A\x46\x0D\x32\xE4\xF7\xE2\x03\x46\xFC\x13\x56"
				"\xFE\xEB\x4A\x52\x50\x06\x53\x6A\x01\x6A\x10\x91\x8B\x46\x18\x96"
				"\x92\x33\xD2\xF7\xF6\x91\xF7\xF6\x42\x87\xCA\xF7\x76\x1A\x8A\xF2"
				"\x8A\xE8\xC0\xCC\x02\x0A\xCC\xB8\x01\x02\x80\x7E\x02\x0E\x75\x04"
				"\xB4\x42\x8B\xF4\x8A\x56\x24\xCD\x13\x61\x61\x72\x0B\x40\x75\x01"
				"\x42\x03\x5E\x0B\x49\x75\x06\xF8\xC3\x41\xBB\x00\x00\x60\x66\x6A"
				"\x00\xEB\xB0\x42\x4F\x4F\x54\x4D\x47\x52\x20\x20\x20\x20\x0D\x0A"
				"\x52\x65\x6D\x6F\x76\x65\x20\x64\x69\x73\x6B\x73\x20\x6F\x72\x20"
				"\x6F\x74\x68\x65\x72\x20\x6D\x65\x64\x69\x61\x2E\xFF\x0D\x0A\x44"
				"\x69\x73\x6B\x20\x65\x72\x72\x6F\x72\xFF\x0D\x0A\x50\x72\x65\x73"
				"\x73\x20\x61\x6E\x79\x20\x6B\x65\x79\x20\x74\x6F\x20\x72\x65\x73"
				"\x74\x61\x72\x74\x0D\x0A\x00\x00\x00\x00\x00\x00\x00\xAC\xCB\xD8";
			phat->end_of_cluster_chain = 0xFFF8;
			break;
		}
		memcpy(dbr->boot_code, boot_code, 448);
		dbr->boot_sector_signature = 0xAA55;
		Phat_SetCachedSectorModified(cached_sector);

		phat->root_dir_cluster = 0;
		phat->data_start_LBA = phat->root_dir_start_LBA + (((LBA_t)root_dir_entry_count * 32) + 511) / 512;
		phat->has_FSInfo = 0;

		for (uint16_t i = 0; i < num_root_dir_sectors; i++)
		{
			ret = Phat_WriteSectorsWithoutCache(phat, partition_start_LBA + phat->root_dir_start_LBA + i, 1, empty_sector);
			if (ret != PhatState_OK) return ret;
		}
	}
	else
	{
		Phat_DBR_FAT32_t dbr_buf;
		Phat_DBR_FAT32_p dbr = (Phat_DBR_FAT32_p)&cached_sector->data;
		Phat_FSInfo_p fsi;
		dbr->jump_boot[0] = 0xEB;
		dbr->jump_boot[1] = 0x58;
		dbr->jump_boot[2] = 0x90;
		memcpy(dbr->OEM_name, "MSDOS5.0", 8);
		dbr->bytes_per_sector = 512;
		dbr->sectors_per_cluster = sectors_per_cluster;
		dbr->reserved_sector_count = reserved_sector_count;
		dbr->num_FATs = num_FATs;
		dbr->root_dir_entry_count = 0;
		dbr->total_sectors_16 = partition_size_in_sectors <= 0xFFFF ? partition_size_in_sectors : 0;
		dbr->media = media_type;
		dbr->FAT_size_16 = FAT_size <= 0xFFFF ? FAT_size : 0;
		dbr->sectors_per_track = sectors_per_track;
		dbr->num_heads = num_heads;
		dbr->hidden_sectors = (uint32_t)partition_start_LBA;
		dbr->total_sectors_32 = partition_size_in_sectors > 0xFFFF ? partition_size_in_sectors : 0;
		dbr->FAT_size_32 = FAT_size > 0xFFFF ? FAT_size : 0;
		dbr->FATs_are_different = 0;
		dbr->version = 0;
		dbr->root_dir_cluster = 2;
		dbr->FS_info_sector = 1;
		dbr->backup_boot_sector = 6;
		memset(dbr->reserved, 0, sizeof dbr->reserved);
		dbr->BIOS_drive_number = BIOS_drive_number;
		dbr->extension_flag = 0x29;
		dbr->volume_ID = volume_ID;
		memset(dbr->volume_label, 0x20, sizeof dbr->volume_label);
		memcpy(dbr->volume_label, volume_lable, strlen(volume_lable));
		memcpy(dbr->file_system_type, "FAT32   ", 8);
		boot_code =
			"\x33\xC9\x8E\xD1\xBC\xF4\x7B\x8E\xC1\x8E\xD9\xBD\x00\x7C\x88\x56"
			"\x40\x88\x4E\x02\x8A\x56\x40\xB4\x41\xBB\xAA\x55\xCD\x13\x72\x10"
			"\x81\xFB\x55\xAA\x75\x0A\xF6\xC1\x01\x74\x05\xFE\x46\x02\xEB\x2D"
			"\x8A\x56\x40\xB4\x08\xCD\x13\x73\x05\xB9\xFF\xFF\x8A\xF1\x66\x0F"
			"\xB6\xC6\x40\x66\x0F\xB6\xD1\x80\xE2\x3F\xF7\xE2\x86\xCD\xC0\xED"
			"\x06\x41\x66\x0F\xB7\xC9\x66\xF7\xE1\x66\x89\x46\xF8\x83\x7E\x16"
			"\x00\x75\x39\x83\x7E\x2A\x00\x77\x33\x66\x8B\x46\x1C\x66\x83\xC0"
			"\x0C\xBB\x00\x80\xB9\x01\x00\xE8\x2C\x00\xE9\xA8\x03\xA1\xF8\x7D"
			"\x80\xC4\x7C\x8B\xF0\xAC\x84\xC0\x74\x17\x3C\xFF\x74\x09\xB4\x0E"
			"\xBB\x07\x00\xCD\x10\xEB\xEE\xA1\xFA\x7D\xEB\xE4\xA1\x7D\x80\xEB"
			"\xDF\x98\xCD\x16\xCD\x19\x66\x60\x80\x7E\x02\x00\x0F\x84\x20\x00"
			"\x66\x6A\x00\x66\x50\x06\x53\x66\x68\x10\x00\x01\x00\xB4\x42\x8A"
			"\x56\x40\x8B\xF4\xCD\x13\x66\x58\x66\x58\x66\x58\x66\x58\xEB\x33"
			"\x66\x3B\x46\xF8\x72\x03\xF9\xEB\x2A\x66\x33\xD2\x66\x0F\xB7\x4E"
			"\x18\x66\xF7\xF1\xFE\xC2\x8A\xCA\x66\x8B\xD0\x66\xC1\xEA\x10\xF7"
			"\x76\x1A\x86\xD6\x8A\x56\x40\x8A\xE8\xC0\xE4\x06\x0A\xCC\xB8\x01"
			"\x02\xCD\x13\x66\x61\x0F\x82\x74\xFF\x81\xC3\x00\x02\x66\x40\x49"
			"\x75\x94\xC3\x42\x4F\x4F\x54\x4D\x47\x52\x20\x20\x20\x20\x00\x00"
			"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
			"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
			"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
			"\x00\x00\x0D\x0A\x44\x69\x73\x6B\x20\x65\x72\x72\x6F\x72\xFF\x0D"
			"\x0A\x50\x72\x65\x73\x73\x20\x61\x6E\x79\x20\x6B\x65\x79\x20\x74"
			"\x6F\x20\x72\x65\x73\x74\x61\x72\x74\x0D\x0A\x00\x00\x00\x00\x00"
			"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
			"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xAC\x01"
			"\xB9\x01\x00\x00";
		dbr->boot_sector_signature = 0xAA55;
		memcpy(&dbr_buf, dbr, 512);
		Phat_SetCachedSectorModified(cached_sector);

		// FS Info
		ret = Phat_ReadSectorThroughCache(phat, partition_start_LBA + 1, &cached_sector);
		if (ret != PhatState_OK) return ret;
		fsi = (Phat_FSInfo_p)&cached_sector->data;
		fsi->lead_signature = 0x41615252;
		memset(fsi->reserved1, 0, sizeof fsi->reserved1);
		fsi->struct_signature = 0x61417272;
		fsi->free_cluster_count = free_clusters;
		fsi->next_free_cluster = 3;
		memset(fsi->reserved2, 0, sizeof fsi->reserved2);
		fsi->trail_signature = 0xAA55;
		Phat_SetCachedSectorModified(cached_sector);

		// Next sector to the FS Info sector
		ret = Phat_ReadSectorThroughCache(phat, partition_start_LBA + 2, &cached_sector);
		if (ret != PhatState_OK) return ret;
		memset(cached_sector->data, 0, 510);
		cached_sector->data[510] = 0x55;
		cached_sector->data[511] = 0xAA;
		Phat_SetCachedSectorModified(cached_sector);

		// Backup DBR
		ret = Phat_ReadSectorThroughCache(phat, partition_start_LBA + 6, &cached_sector);
		if (ret != PhatState_OK) return ret;
		memcpy(cached_sector->data, &dbr_buf, 512);
		Phat_SetCachedSectorModified(cached_sector);

		phat->root_dir_cluster = 2;
		phat->data_start_LBA = phat->root_dir_start_LBA;
		phat->end_of_cluster_chain = 0x0FFFFFF8;
		phat->has_FSInfo = 1;

		// Wipe for FAT32 root dir
		ret = Phat_WipeCluster(phat, 2);
		if (ret != PhatState_OK) return ret;

		// End of the root dir cluster chain
		ret = Phat_WriteFAT(phat, 2, phat->end_of_cluster_chain, 0);
		if (ret != PhatState_OK) return ret;
	}

	// Initialize the FAT table
	for (uint8_t i = 0; i < num_FATs; i++)
	{
		LBA_t FAT_LBA = partition_start_LBA + phat->FAT1_start_LBA + FAT_size * i;
		ret = Phat_ReadSectorThroughCache(phat, FAT_LBA, &cached_sector);
		if (ret != PhatState_OK) return ret;

		memset(cached_sector->data, 0, 512);
		switch (FAT_bits)
		{
		case 12:
			*(uint32_t *)&cached_sector->data[0] = 0x00FFFFF0;
			break;
		case 16:
			*(uint32_t *)&cached_sector->data[0] = 0xFFFFFFF8;
			break;
		case 32:
			*(uint32_t *)&cached_sector->data[0] = 0x0FFFFFF8;
			*(uint32_t *)&cached_sector->data[4] = 0xFFFFFFFF;
			break;
		}

		Phat_SetCachedSectorModified(cached_sector);
		for (uint32_t f = 1; f < FAT_size; f++)
		{
			ret = Phat_WriteSectorsWithoutCache(phat, FAT_LBA + f, 1, empty_sector);
			if (ret != PhatState_OK) return ret;
		}
	}

	ret = Phat_FlushCache(phat);
	if (ret != PhatState_OK) return ret;

	if (sectors_per_cluster > 8) return PhatState_FSIsSubOptimal;
	return PhatState_OK;
}
