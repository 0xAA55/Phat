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

#ifndef MAX_LFN
#define MAX_LFN 256
#endif

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
	LBA_t root_dir_cluster;
	LBA_t data_start_LBA;
	LBA_t root_dir_start_LBA;
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
}Phat_t, *Phat_p;

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

typedef struct Phat_DirInfo_s
{
	Phat_p phat;
	uint32_t dir_start_cluster;
	uint32_t dir_current_cluster;
	uint32_t cur_diritem_in_cur_cluster;
	uint8_t file_name_8_3[11];
	uint8_t attributes;
	Phat_Date_t cdate;
	Phat_Time_t ctime;
	Phat_Date_t mdate;
	Phat_Time_t mtime;
	Phat_Date_t adate;
	uint8_t checksum;
	uint32_t file_size;
	uint32_t first_cluster;
	WChar_t LFN_name[MAX_LFN];
	uint16_t LFN_length;
}Phat_DirInfo_t, *Phat_DirInfo_p;

typedef struct Phat_FileInfo_s
{
	Phat_p phat;
	uint32_t first_cluster;
	uint32_t file_pointer;
	uint32_t cur_cluster;
	uint32_t cur_cluster_index;
	uint32_t file_size;
	PhatBool_t readonly;
	uint8_t sector_buffer[512];
	LBA_t buffer_LBA;
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
	PhatState_DirectoryNotFound,
	PhatState_FileNotFound,
	PhatState_IsADirectory,
	PhatState_NotADirectory,
	PhatState_InvalidPath,
	PhatState_EndOfDirectory,
	PhatState_EndOfFATChain,
	PhatState_EndOfFile,
	PhatState_NotEnoughSpace,
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

PhatState Phat_Mount(Phat_p phat, int partition_index);
PhatState Phat_FlushCache(Phat_p phat);
PhatState Phat_Unmount(Phat_p phat);
PhatState Phat_OpenDir(Phat_p phat, WChar_p path, Phat_DirInfo_p dir_info);
PhatState Phat_NextDirItem(Phat_DirInfo_p dir_info);
void Phat_CloseDir(Phat_DirInfo_p dir_info);

PhatState Phat_OpenFile(Phat_p phat, WChar_p path, PhatBool_t readonly, Phat_FileInfo_p file_info);
PhatState Phat_ReadFile(Phat_FileInfo_p file_info, void *buffer, uint32_t bytes_to_read, uint32_t *bytes_read);
void Phat_CloseFile(Phat_FileInfo_p file_info);

PhatState Phat_SeekFile(Phat_FileInfo_p file_info, uint32_t position);
void Phat_GetFilePointer(Phat_FileInfo_p file_info, uint32_t *position);
void Phat_GetFileSize(Phat_FileInfo_p file_info, uint32_t *size);
#endif
