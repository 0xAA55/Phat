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

#ifndef PHAT_ALIGNMENT
#define PHAT_ALIGNMENT __attribute__((aligned(4)))
#endif

typedef uint16_t WChar_t, *WChar_p;
typedef uint32_t Cluster_t, *Cluster_p;
typedef int32_t SCluster_t, *SCluster_p;
typedef Cluster_t FileSize_t, *FileSize_p;

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
}PHAT_ALIGNMENT Phat_Date_t, *Phat_Date_p;

typedef struct Phat_Time_s
{
	uint8_t hours;
	uint8_t minutes;
	uint8_t seconds;
	uint16_t milliseconds;
}PHAT_ALIGNMENT Phat_Time_t, *Phat_Time_p;

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
	uint16_t root_dir_entry_count;
	PhatBool_t FATs_are_same;
	PhatBool_t is_dirty;
	PhatBool_t write_enable;
	uint8_t FAT_bits;
	uint8_t num_FATs;
	uint16_t bytes_per_sector;
	uint8_t sectors_per_cluster;
	uint8_t num_diritems_in_a_sector;
	uint16_t num_diritems_in_a_cluster;
	Cluster_t num_FAT_entries;
	PhatBool_t has_FSInfo;
	Cluster_t free_clusters;
	Cluster_t next_free_cluster;
	Cluster_t max_valid_cluster;
	Cluster_t end_of_cluster_chain;
}PHAT_ALIGNMENT Phat_t, *Phat_p;

typedef struct Phat_DirInfo_s
{
	Phat_p phat;
	Cluster_t dir_start_cluster;
	Cluster_t dir_current_cluster;
	Cluster_t dir_current_cluster_index;
	Cluster_t cur_diritem;
	uint8_t file_name_8_3[11];
	uint8_t attributes;
	Phat_Date_t cdate;
	Phat_Time_t ctime;
	Phat_Date_t mdate;
	Phat_Time_t mtime;
	Phat_Date_t adate;
	uint8_t sfn_checksum;
	Cluster_t file_size;
	Cluster_t first_cluster;
	WChar_t LFN_name[MAX_LFN + 1];
	uint16_t LFN_length;
}PHAT_ALIGNMENT Phat_DirInfo_t, *Phat_DirInfo_p;

typedef struct Phat_FileInfo_s
{
	Phat_p phat;
	Phat_DirInfo_t file_item;
	Cluster_t first_cluster;
	Cluster_t file_pointer;
	Cluster_t cur_cluster;
	Cluster_t cur_cluster_index;
	Cluster_t file_size;
	uint8_t offset_in_cluster;
	PhatBool_t readonly;
	PhatBool_t modified;
	PhatBool_t sector_buffer_is_valid;
	uint8_t sector_buffer[512];
	LBA_t sector_buffer_LBA;
}PHAT_ALIGNMENT Phat_FileInfo_t, *Phat_FileInfo_p;

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
	PhatState_CannotMakeFS,
	PhatState_FSIsSubOptimal,
	PhatState_DiskAlreadyInitialized,
	PhatState_NoFreePartitions,
	PhatState_PartitionTooSmall,
	PhatState_NoMBR,
	PhatState_PartitionLBAIsIllegal,
	PhatState_PartitionOverlapped,
	PhatState_PartitionIndexOutOfBound,
	PhatState_NeedBigLBA,
	PhatState_LastState,
}PhatState;

#define ATTRIB_READ_ONLY 0x01
#define ATTRIB_HIDDEN 0x02
#define ATTRIB_SYSTEM 0x04
#define ATTRIB_VOLUME_ID 0x08
#define ATTRIB_DIRECTORY 0x10
#define ATTRIB_ARCHIVE 0x20

/**
 * @brief Initialize the Phat filesystem context
 *
 * @param phat Pointer to the Phat context structure to initialize
 * @return PhatState
 *   - PhatState_OK: Success
 *   - PhatState_InvalidParameter: phat is NULL
 *   - PhatState_DriverError: Failed to open underlying storage device
 *
 * @note This function must be called before any other Phat operations.
 * It initializes the disk driver and sets up internal structures.
 */
PHAT_FUNC PhatState Phat_Init(Phat_p phat);

/**
 * @brief Deinitialize Phat context and close storage device
 *
 * @param phat Initialized Phat context
 * @return PhatState
 *   - PhatState_OK: Success
 *   - PhatState_InvalidParameter: phat is NULL
 *   - PhatState_WriteFail: Failed to flush cache during unmount
 *
 * @note Calls Phat_Unmount internally. Context becomes invalid after this call.
 */
PHAT_FUNC PhatState Phat_DeInit(Phat_p phat);

/**
 * @brief Convert PhatState code to human-readable string
 *
 * @param s Error code to convert
 * @return const char* Descriptive string
 */
PHAT_FUNC const char *Phat_StateToString(PhatState s);

/**
 * @brief Navigate to parent directory by truncating the path
 *
 * @param path Path to modify (modified in-place)
 *
 * @note Removes the last path component:
 * - "/a/b" -> "/a"
 * - "/a/b/" -> "/a"
 * - "/a" -> "/"
 * - "/" -> "/" (no change)
 *
 * @example
 * WChar_t path[] = L"/dir/subdir/file.txt";
 * Phat_ToUpperDirectoryPath(path); // path becomes L"/dir/subdir"
 */
PHAT_FUNC void Phat_ToUpperDirectoryPath(WChar_p path);

/**
 * @brief Normalize path by removing redundant elements
 *
 * @param path Path to normalize (modified in-place)
 *
 * @note Removes "./", "../", and redundant slashes.
 * Example: "a/./b/../c" becomes "a/c".
 */
PHAT_FUNC void Phat_NormalizePath(WChar_p path);

/**
 * @brief Extract filename from path
 *
 * @param path Full path
 * @param name Buffer to store extracted filename
 *
 * @note Extracts the last component of the path.
 * Example: "dir/sub/file.txt" -> "file.txt".
 */
PHAT_FUNC void Phat_PathToName(WChar_p path, WChar_p name);

/**
 * @brief Extract filename from path (in-place)
 *
 * @param path Full path (modified to contain only filename)
 *
 * @note Same as Phat_PathToName but modifies the original path.
 */
PHAT_FUNC void Phat_PathToNameInPlace(WChar_p path);

/**
 * @brief Check if filename contains valid characters
 *
 * @param filename Name to validate
 * @return PhatBool_t Non-zero if valid, zero if invalid
 *
 * @note Checks for illegal characters: " * / : < > ? \ | and control chars.
 * Also rejects "." and "..".
 */
PHAT_FUNC PhatBool_t Phat_IsValidFilename(WChar_p filename);

/**
 * @brief Mount a filesystem from specified partition
 *
 * @param phat Initialized Phat context
 * @param partition_index Partition number to mount (0-based)
 * @param write_enable Enable write operations if non-zero
 * @return PhatState
 *   - PhatState_OK: Success
 *   - PhatState_InvalidParameter: phat is NULL
 *   - PhatState_NoMBR: No valid partition table found
 *   - PhatState_FSNotFat: Partition doesn't contain FAT filesystem
 *   - PhatState_PartitionIndexOutOfBound: Invalid partition index
 *   - PhatState_ReadFail: Failed to read partition data
 *
 * @note If partition_index is 0 and no partition table exists,
 *       the entire disk is treated as a single FAT partition.
 */
PHAT_FUNC PhatState Phat_Mount(Phat_p phat, int partition_index, PhatBool_t write_enable);

/**
 * @brief Flush all cached sectors to storage
 *
 * @param phat Mounted Phat context
 * @return PhatState
 *   - PhatState_OK: Success
 *   - PhatState_InvalidParameter: phat is NULL
 *   - PhatState_WriteFail: Failed to write cached data
 *
 * @note This ensures all pending writes are committed to storage.
 * Called automatically during Unmount/DeInit.
 */
PHAT_FUNC PhatState Phat_FlushCache(Phat_p phat, PhatBool_t invalidate);

/**
 * @brief Unmount the filesystem
 *
 * @param phat Mounted Phat context
 * @return PhatState
 *   - PhatState_OK: Success
 *   - PhatState_InvalidParameter: phat is NULL
 *   - PhatState_WriteFail: Failed to flush cache
 *
 * @note Flushes cache and marks filesystem as clean.
 * The context can be reused for mounting another partition.
 */
PHAT_FUNC PhatState Phat_Unmount(Phat_p phat);

/**
 * @brief Set current date and time for file system operations
 *
 * @param phat Mounted Phat context
 * @param cur_date Current date (can be NULL to keep existing)
 * @param cur_time Current time (can be NULL to keep existing)
 *
 * @note This date/time will be used for:
 * - File creation timestamps
 * - File modification timestamps
 * - Directory entry timestamps
 *
 * @example
 * Phat_Date_t date = {2026, 1, 6};
 * Phat_Time_t time = {14, 30, 0, 0};
 * Phat_SetCurDateTime(phat, &date, &time);
 */
PHAT_FUNC void Phat_SetCurDateTime(Phat_p phat, const Phat_Date_p cur_date, const Phat_Time_p cur_time);

/**
 * @brief Open root directory for iteration
 *
 * @param phat Mounted Phat context
 * @param dir_info Directory info structure to initialize
 *
 * @note Prepares dir_info for iterating through root directory items.
 * dir_info must be closed with Phat_CloseDir when done.
 */
PHAT_FUNC void Phat_OpenRootDir(Phat_p phat, Phat_DirInfo_p dir_info);

/**
 * @brief Change current directory
 *
 * @param dir_info Current directory context
 * @param dirname Subdirectory name to navigate to
 * @return PhatState
 *   - PhatState_OK: Success
 *   - PhatState_InvalidParameter: Invalid parameters
 *   - PhatState_DirectoryNotFound: Subdirectory doesn't exist
 *   - PhatState_NotADirectory: Target exists but is not a directory
 *
 * @note Updates dir_info to point to the subdirectory.
 * Similar to 'cd' command in shell.
 */
PHAT_FUNC PhatState Phat_ChDir(Phat_DirInfo_p dir_info, const WChar_p dirname);

/**
 * @brief Open directory at specified path
 *
 * @param phat Mounted Phat context
 * @param path Directory path (UTF-16 encoded)
 * @param dir_info Directory info structure to initialize
 * @return PhatState
 *   - PhatState_OK: Success
 *   - PhatState_InvalidParameter: Invalid parameters
 *   - PhatState_DirectoryNotFound: Path doesn't exist
 *   - PhatState_NotADirectory: Path exists but is not a directory
 *
 * @example Phat_OpenDir(phat, L"SubDir/SubSubDir", &dir_info);
 */
PHAT_FUNC PhatState Phat_OpenDir(Phat_p phat, const WChar_p path, Phat_DirInfo_p dir_info);

/**
 * @brief Get next item in opened directory
 *
 * @param dir_info Opened directory context
 * @return PhatState
 *   - PhatState_OK: Success, dir_info populated with next item
 *   - PhatState_EndOfDirectory: No more items
 *   - PhatState_InvalidParameter: dir_info is NULL or invalid
 *   - PhatState_ReadFail: Failed to read directory data
 *
 * @note Populates dir_info with information about the next directory entry.
 * LFN_name, attributes, file_size, first_cluster, etc. are updated.
 */
PHAT_FUNC PhatState Phat_NextDirItem(Phat_DirInfo_p dir_info);

/**
 * @brief Close directory context
 *
 * @param dir_info Directory context to close
 *
 * @note Releases resources associated with directory iteration.
 * dir_info becomes invalid after this call.
 */
PHAT_FUNC void Phat_CloseDir(Phat_DirInfo_p dir_info);

/**
 * @brief Open file for reading or writing
 *
 * @param dir_info Opened directory context
 * @param path File path (UTF-16 encoded)
 * @param readonly Open in read-only mode if non-zero
 * @param file_info File info structure to initialize
 * @return PhatState
 *   - PhatState_OK: Success
 *   - PhatState_InvalidParameter: Invalid parameters
 *   - PhatState_FileNotFound: File doesn't exist (and readonly=1)
 *   - PhatState_IsADirectory: Path exists but is a directory
 *   - PhatState_ReadOnly: Write attempted on read-only filesystem
 *
 * @note If readonly=0 and file doesn't exist, it will be created.
 * file_info must be closed with Phat_CloseFile when done.
 */
PHAT_FUNC PhatState Phat_OpenFile(Phat_DirInfo_p dir_info, const WChar_p path, PhatBool_t readonly, Phat_FileInfo_p file_info);

/**
 * @brief Open file from root dir for reading or writing
 *
 * @param phat Mounted Phat context
 * @param path File path (UTF-16 encoded)
 * @param readonly Open in read-only mode if non-zero
 * @param file_info File info structure to initialize
 * @return PhatState
 *   - PhatState_OK: Success
 *   - PhatState_InvalidParameter: Invalid parameters
 *   - PhatState_FileNotFound: File doesn't exist (and readonly=1)
 *   - PhatState_IsADirectory: Path exists but is a directory
 *   - PhatState_ReadOnly: Write attempted on read-only filesystem
 *
 * @note If readonly=0 and file doesn't exist, it will be created.
 * file_info must be closed with Phat_CloseFile when done.
 */
PHAT_FUNC PhatState Phat_OpenFileFromRoot(Phat_p phat, const WChar_p path, PhatBool_t readonly, Phat_FileInfo_p file_info);

/**
 * @brief Read data from opened file
 *
 * @param file_info Opened file context
 * @param buffer Buffer to store read data
 * @param bytes_to_read Number of bytes to read
 * @param bytes_read Actual number of bytes read (can be NULL)
 * @return PhatState
 *   - PhatState_OK: Success, more data available
 *   - PhatState_EndOfFile: Reached end of file
 *   - PhatState_InvalidParameter: Invalid parameters
 *   - PhatState_ReadFail: Storage read error
 *
 * @note Reading continues from current file pointer position.
 * bytes_read indicates how many bytes were actually read.
 */
PHAT_FUNC PhatState Phat_ReadFile(Phat_FileInfo_p file_info, void *buffer, size_t bytes_to_read, size_t *bytes_read);

/**
 * @brief Write data to opened file
 *
 * @param file_info Opened file context (must be writable)
 * @param buffer Data to write
 * @param bytes_to_write Number of bytes to write
 * @param bytes_written Actual number of bytes written (can be NULL)
 * @return PhatState
 *   - PhatState_OK: Success
 *   - PhatState_InvalidParameter: Invalid parameters
 *   - PhatState_ReadOnly: File or filesystem is read-only
 *   - PhatState_WriteFail: Storage write error
 *   - PhatState_NotEnoughSpace: Insufficient disk space
 *
 * @note Writing continues from current file pointer position.
 * File size is automatically extended if writing beyond current EOF.
 */
PHAT_FUNC PhatState Phat_WriteFile(Phat_FileInfo_p file_info, const void *buffer, size_t bytes_to_write, size_t *bytes_written);

/**
 * @brief Close file and update directory entry
 *
 * @param file_info File context to close
 * @return PhatState
 *   - PhatState_OK: Success
 *   - PhatState_InvalidParameter: file_info is NULL
 *   - PhatState_WriteFail: Failed to update directory entry
 *
 * @note Updates file metadata (size, modification time) in directory.
 * file_info becomes invalid after this call.
 */
PHAT_FUNC PhatState Phat_CloseFile(Phat_FileInfo_p file_info);

/**
 * @brief Set file pointer position
 *
 * @param file_info Opened file context
 * @param position New position (0 = beginning of file)
 * @return PhatState
 *   - PhatState_OK: Success
 *   - PhatState_EndOfFile: Position is beyond EOF (allowed)
 *   - PhatState_InvalidParameter: file_info is NULL
 *
 * @note Subsequent read/write operations will start from this position.
 */
PHAT_FUNC PhatState Phat_SeekFile(Phat_FileInfo_p file_info, FileSize_t position);

/**
 * @brief Get current file pointer position
 *
 * @param file_info Opened file context
 * @param position Current position is stored here
 */
PHAT_FUNC void Phat_GetFilePointer(Phat_FileInfo_p file_info, FileSize_t *position);

/**
 * @brief Get file size
 *
 * @param file_info Opened file context
 * @param size File size is stored here
 */
PHAT_FUNC void Phat_GetFileSize(Phat_FileInfo_p file_info, FileSize_t *size);

/**
 * @brief Check if file pointer is at end of file
 *
 * @param file_info Opened file context
 * @return PhatBool_t Non-zero if at EOF, zero otherwise
 */
PHAT_FUNC PhatBool_t Phat_IsEOF(Phat_FileInfo_p file_info);

/**
 * @brief Create new directory
 *
 * @param phat Mounted Phat context (must be writable)
 * @param path Directory path to create
 * @return PhatState
 *   - PhatState_OK: Success
 *   - PhatState_InvalidParameter: Invalid parameters
 *   - PhatState_ReadOnly: Filesystem is read-only
 *   - PhatState_DirectoryAlreadyExists: Directory already exists
 *   - PhatState_FileAlreadyExists: Path exists as file
 *   - PhatState_NameTooLong: Path too long
 *   - PhatState_BadFileName: Invalid characters in name
 *   - PhatState_NotEnoughSpace: Insufficient disk space
 *
 * @note Creates all intermediate directories if they don't exist (like mkdir -p).
 */
PHAT_FUNC PhatState Phat_CreateDirectory(Phat_p phat, const WChar_p path);

/**
 * @brief Remove empty directory
 *
 * @param phat Mounted Phat context (must be writable)
 * @param path Directory path to remove
 * @return PhatState
 *   - PhatState_OK: Success
 *   - PhatState_InvalidParameter: Invalid parameters
 *   - PhatState_ReadOnly: Filesystem is read-only
 *   - PhatState_DirectoryNotFound: Directory doesn't exist
 *   - PhatState_NotADirectory: Path exists but is not a directory
 *   - PhatState_DirectoryNotEmpty: Directory contains files/subdirectories
 *
 * @note Directory must be empty before removal.
 */
PHAT_FUNC PhatState Phat_RemoveDirectory(Phat_p phat, const WChar_p path);

/**
 * @brief Delete file
 *
 * @param phat Mounted Phat context (must be writable)
 * @param path File path to delete
 * @return PhatState
 *   - PhatState_OK: Success
 *   - PhatState_InvalidParameter: Invalid parameters
 *   - PhatState_ReadOnly: Filesystem is read-only
 *   - PhatState_FileNotFound: File doesn't exist
 *   - PhatState_IsADirectory: Path exists but is a directory
 *
 * @note Marks file as deleted and frees allocated clusters.
 */
PHAT_FUNC PhatState Phat_DeleteFile(Phat_p phat, const WChar_p path);

/**
 * @brief Rename file or directory
 *
 * @param phat Mounted Phat context (must be writable)
 * @param path Old path
 * @param new_name New name (in same directory)
 * @return PhatState
 *   - PhatState_OK: Success
 *   - PhatState_InvalidParameter: Invalid parameters
 *   - PhatState_ReadOnly: Filesystem is read-only
 *   - PhatState_FileNotFound/DirectoryNotFound: Source doesn't exist
 *   - PhatState_FileAlreadyExists/DirectoryAlreadyExists: Target name exists
 *   - PhatState_BadFileName: Invalid characters in new name
 *
 * @note Only renames within the same directory. Use Phat_Move for moving between directories.
 */
PHAT_FUNC PhatState Phat_Rename(Phat_p phat, const WChar_p path, const WChar_p new_name);

/**
 * @brief Move file or directory to different location
 *
 * @param phat Mounted Phat context (must be writable)
 * @param oldpath Source path
 * @param newpath Destination path
 * @return PhatState
 *   - PhatState_OK: Success
 *   - PhatState_InvalidParameter: Invalid parameters
 *   - PhatState_ReadOnly: Filesystem is read-only
 *   - PhatState_FileNotFound/DirectoryNotFound: Source doesn't exist
 *   - PhatState_FileAlreadyExists/DirectoryAlreadyExists: Target exists
 *   - PhatState_InvalidPath: Attempt to move directory into its own subdirectory
 *
 * @note Can move files/directories between different directories.
 */
PHAT_FUNC PhatState Phat_Move(Phat_p phat, const WChar_p oldpath, const WChar_p newpath);

/**
 * @brief Initialize disk with MBR partition table
 *
 * @param phat Phat context with opened storage device
 * @param force Overwrite existing MBR if non-zero
 * @param flush Write changes immediately if non-zero
 * @return PhatState
 *   - PhatState_OK: Success
 *   - PhatState_InvalidParameter: phat is NULL
 *   - PhatState_DiskAlreadyInitialized: MBR exists and force=0
 *   - PhatState_WriteFail: Failed to write MBR
 */
PHAT_FUNC PhatState Phat_InitializeMBR(Phat_p phat, PhatBool_t force, PhatBool_t flush);

/**
 * @brief Initialize disk with GPT partition table
 *
 * @param phat Phat context with opened storage device
 * @param force Overwrite existing GPT if non-zero
 * @param flush Write changes immediately if non-zero
 * @return PhatState
 *   - PhatState_OK: Success
 *   - PhatState_InvalidParameter: phat is NULL
 *   - PhatState_DiskAlreadyInitialized: GPT exists and force=0
 *   - PhatState_WriteFail: Failed to write GPT structures
 *   - PhatState_NeedBigLBA: Device requires 64-bit LBA addressing
 */
PHAT_FUNC PhatState Phat_InitializeGPT(Phat_p phat, PhatBool_t force, PhatBool_t flush);

/**
 * @brief Get usable LBA range for creating partitions
 *
 * @param phat Phat context with opened storage device
 * @param first First usable LBA address
 * @param last Last usable LBA address
 * @return PhatState
 *   - PhatState_OK: Success
 *   - PhatState_InvalidParameter: phat, first, or last is NULL
 *   - PhatState_NoMBR: No valid partition table found
 */
PHAT_FUNC PhatState Phat_GetFirstAndLastUsableLBA(Phat_p phat, LBA_p first, LBA_p last);

/**
 * @brief Create new partition
 *
 * @param phat Phat context with opened storage device
 * @param partition_start Starting LBA of partition
 * @param partition_size_in_sectors Size of partition in sectors
 * @param bootable Mark partition as bootable if non-zero
 * @param flush Write changes immediately if non-zero
 * @return PhatState
 *   - PhatState_OK: Success
 *   - PhatState_InvalidParameter: Invalid parameters
 *   - PhatState_NoMBR: No partition table exists
 *   - PhatState_PartitionLBAIsIllegal: LBA outside usable range
 *   - PhatState_PartitionOverlapped: Overlaps existing partition
 *   - PhatState_NoFreePartitions: No free partition slots available
 */
PHAT_FUNC PhatState Phat_CreatePartition(Phat_p phat, LBA_t partition_start, LBA_t partition_size_in_sectors, PhatBool_t bootable, PhatBool_t flush);

/**
 * @brief Format partition and mount it
 *
 * @param phat Phat context with opened storage device
 * @param partition_index Partition to format
 * @param FAT_bits FAT type (12, 16, 32, or 0 for auto-detect)
 * @param root_dir_entry_count Root directory entries (0 for FAT32)
 * @param volume_ID Volume ID (serial number)
 * @param volume_label Volume label (NULL for default)
 * @param flush Write changes immediately if non-zero
 * @return PhatState
 *   - PhatState_OK: Success
 *   - PhatState_FSIsSubOptimal: Filesystem created but with suboptimal parameters
 *   - PhatState_InvalidParameter: Invalid parameters
 *   - PhatState_CannotMakeFS: Partition too small for FAT
 *   - PhatState_PartitionTooSmall: Partition too small for selected FAT type
 */
PHAT_FUNC PhatState Phat_MakeFS_And_Mount(Phat_p phat, int partition_index, int FAT_bits, uint16_t root_dir_entry_count, uint32_t volume_ID, const char *volume_lable, PhatBool_t flush);
#endif
