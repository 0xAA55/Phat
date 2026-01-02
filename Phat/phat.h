#ifndef _PHAT_H_
#define _PHAT_H_ 1

#include <stdint.h>

#include "BSP_phat.h"

#ifndef PHAT_CACHED_SECTORS
#define PHAT_CACHED_SECTORS 8
#endif

#define SECTORCACHE_SYNC 0x80000000
#define SECTORCACHE_VALID 0x40000000

#ifndef MAX_LFN
#define MAX_LFN 255
#endif

#ifndef PHAT_DEFAULT_YEAR
#define PHAT_DEFAULT_YEAR 2026
#endif

#ifndef PHAT_DEFAULT_MONTH
#define PHAT_DEFAULT_MONTH 1
#endif

#ifndef PHAT_DEFAULT_DAY
#define PHAT_DEFAULT_DAY 1
#endif

#ifndef PHAT_DEFAULT_HOUR
#define PHAT_DEFAULT_HOUR 12
#endif

#ifndef PHAT_DEFAULT_MINUTE
#define PHAT_DEFAULT_MINUTE 0
#endif

#ifndef PHAT_DEFAULT_SECOND
#define PHAT_DEFAULT_SECOND 0
#endif

typedef uint16_t WChar_t, *WChar_p;

typedef struct Phat_SectorCache_s
{
	uint8_t data[512];
	LBA_t	LBA;
	uint32_t usage;
	struct Phat_SectorCache_s *prev;
	struct Phat_SectorCache_s *next;
}Phat_SectorCache_t, *Phat_SectorCache_p;

typedef struct Phat_Date_s
{
	uint16_t year;
	uint8_t month;
	uint8_t day;
}Phat_Date_t, *Phat_Date_p;

typedef struct Phat_Time_s
{
	uint8_t hours;
	uint8_t minutes;
	uint8_t seconds;
	uint16_t milliseconds;
}Phat_Time_t, *Phat_Time_p;

typedef struct Phat_s
{
	Phat_Disk_Driver_t driver;
	Phat_SectorCache_t cache[PHAT_CACHED_SECTORS];
	Phat_SectorCache_p cache_LRU_head;
	Phat_SectorCache_p cache_LRU_tail;
	WChar_t filename_buffer[MAX_LFN + 1];
	Phat_Date_t cur_date;
	Phat_Time_t cur_time;
	LBA_t partition_start_LBA;
	LBA_t total_sectors;
	LBA_t FAT_size_in_sectors;
	LBA_t FAT1_start_LBA;
	LBA_t root_dir_cluster;
	LBA_t data_start_LBA;
	LBA_t root_dir_start_LBA;
	uint16_t root_entry_count;
	PhatBool_t FATs_are_same;
	PhatBool_t is_dirty;
	uint8_t FAT_bits;
	uint8_t num_FATs;
	uint16_t bytes_per_sector;
	uint8_t sectors_per_cluster;
	uint8_t num_diritems_in_a_sector;
	uint16_t num_diritems_in_a_cluster;
	uint32_t num_FAT_entries;
	PhatBool_t has_FSInfo;
	uint32_t free_clusters;
	uint32_t next_free_cluster;
	uint32_t max_valid_cluster;
	uint32_t end_of_cluster_chain;
}Phat_t, *Phat_p;

typedef struct Phat_DirInfo_s
{
	Phat_p phat;
	uint32_t dir_start_cluster;
	uint32_t dir_current_cluster;
	uint32_t dir_current_cluster_index;
	uint32_t cur_diritem;
	uint8_t file_name_8_3[11];
	uint8_t attributes;
	Phat_Date_t cdate;
	Phat_Time_t ctime;
	Phat_Date_t mdate;
	Phat_Time_t mtime;
	Phat_Date_t adate;
	uint8_t sfn_checksum;
	uint32_t file_size;
	uint32_t first_cluster;
	WChar_t LFN_name[MAX_LFN + 1];
	uint16_t LFN_length;
}Phat_DirInfo_t, *Phat_DirInfo_p;

typedef struct Phat_FileInfo_s
{
	Phat_p phat;
	Phat_DirInfo_t file_item;
	uint32_t first_cluster;
	uint32_t file_pointer;
	uint32_t cur_cluster;
	uint32_t cur_cluster_index;
	uint32_t file_size;
	PhatBool_t readonly;
	PhatBool_t modified;
	uint8_t sector_buffer[512];
	LBA_t sector_buffer_LBA;
}Phat_FileInfo_t, *Phat_FileInfo_p;

typedef enum PhatState_e
{
	PhatState_OK = 0,
	PhatState_InvalidParameter,
	PhatState_InternalError,
	PhatState_DriverError,
	PhatState_ReadFail,
	PhatState_WriteFail,
	PhatState_PartitionTableError,
	PhatState_FSNotFat,
	PhatState_FATError,
	PhatState_FSError,
	PhatState_FileNotFound,
	PhatState_DirectoryNotFound,
	PhatState_IsADirectory,
	PhatState_NotADirectory,
	PhatState_InvalidPath,
	PhatState_EndOfDirectory,
	PhatState_EndOfFATChain,
	PhatState_EndOfFile,
	PhatState_NotEnoughSpace,
	PhatState_DirectoryNotEmpty,
	PhatState_FileAlreadyExists,
	PhatState_DirectoryAlreadyExists,
	PhatState_ReadOnly,
	PhatState_NameTooLong,
	PhatState_BadFileName,
	PhatState_LastState,
}PhatState;

#define ATTRIB_READ_ONLY 0x01
#define ATTRIB_HIDDEN 0x02
#define ATTRIB_SYSTEM 0x04
#define ATTRIB_VOLUME_ID 0x08
#define ATTRIB_DIRECTORY 0x10
#define ATTRIB_ARCHIVE 0x20

PhatState Phat_Init(Phat_p phat);
PhatState Phat_DeInit(Phat_p phat);

void Phat_ToUpperDirectoryPath(WChar_p path);
void Phat_NormalizePath(WChar_p path);
void Phat_PathToName(WChar_p path, WChar_p name);
void Phat_PathToNameInPlace(WChar_p path);
PhatBool_t Phat_IsValidFilename(WChar_p filename);

PhatState Phat_Mount(Phat_p phat, int partition_index);
PhatState Phat_FlushCache(Phat_p phat);
PhatState Phat_Unmount(Phat_p phat);
void Phat_SetCurDateTime(Phat_p phat, Phat_Date_p cur_date, Phat_Time_p cur_time);

void Phat_OpenRootDir(Phat_p phat, Phat_DirInfo_p dir_info);
PhatState Phat_ChDir(Phat_DirInfo_p dir_info, const WChar_p dirname);

PhatState Phat_OpenDir(Phat_p phat, const WChar_p path, Phat_DirInfo_p dir_info);
PhatState Phat_NextDirItem(Phat_DirInfo_p dir_info);
void Phat_CloseDir(Phat_DirInfo_p dir_info);

PhatState Phat_OpenFile(Phat_p phat, const WChar_p path, PhatBool_t readonly, Phat_FileInfo_p file_info);
PhatState Phat_ReadFile(Phat_FileInfo_p file_info, void *buffer, uint32_t bytes_to_read, uint32_t *bytes_read);
PhatState Phat_WriteFile(Phat_FileInfo_p file_info, const void *buffer, uint32_t bytes_to_write, uint32_t *bytes_written);
PhatState Phat_CloseFile(Phat_FileInfo_p file_info);

PhatState Phat_SeekFile(Phat_FileInfo_p file_info, uint32_t position);
void Phat_GetFilePointer(Phat_FileInfo_p file_info, uint32_t *position);
void Phat_GetFileSize(Phat_FileInfo_p file_info, uint32_t *size);
PhatBool_t Phat_IsEOF(Phat_FileInfo_p file_info);

PhatState Phat_CreateDirectory(Phat_p phat, const WChar_p path);
PhatState Phat_RemoveDirectory(Phat_p phat, const WChar_p path);
PhatState Phat_DeleteFile(Phat_p phat, const WChar_p path);

const char *Phat_StateToString(PhatState s);

#endif
