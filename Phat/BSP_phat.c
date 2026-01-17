#include "BSP_Phat.h"

#ifdef _WIN32
#ifndef __weak
#define __weak
#endif
#elif defined(__GNUC__) || (defined (__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050))
#ifndef __weak
#define __weak __attribute__((weak))
#endif
#endif

__weak PhatBool_t BSP_OpenDevice(void *userdata);
__weak PhatBool_t BSP_CloseDevice(void *userdata);
__weak PhatBool_t BSP_ReadSector(void *buffer, LBA_t LBA, size_t num_blocks, void *userdata);
__weak PhatBool_t BSP_WriteSector(const void *buffer, LBA_t LBA, size_t num_blocks, void *userdata);

#ifdef _WIN32
#define INITGUID
#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <Windows.h>
#include <virtdisk.h>
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "VirtDisk.lib")
#pragma comment(lib, "Rpcrt4.lib")

static const WCHAR* BSP_DeviceFilePath = L"test.vhd";

static HANDLE hDevice = INVALID_HANDLE_VALUE;

#pragma pack(push, 1)
typedef struct VHD_Footer_s
{
	char cookie[8];
	uint32_t features;
	uint32_t format_version;
	uint64_t data_offset;
	uint32_t timestamp;
	uint32_t creator_app;
	uint32_t creator_version;
	uint32_t creator_host_os;
	uint64_t original_size;
	uint64_t current_size;
	uint16_t cylinders;
	uint8_t heads;
	uint8_t sectors;
	uint32_t disk_type;
	uint32_t checksum;
	UUID unique_id;
	uint8_t saved_state;
	uint8_t reserved[427];
}VHD_Footer_t, *VHD_Footer_p;
#pragma pack(pop)

static void ShowError(DWORD error_code, const char *performing)
{
	LPWSTR w_message_buffer = NULL;
	LPSTR message_buffer = NULL;

	//Ask Win32 to give us the string version of that message ID.
	//The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
	int num_chars = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&w_message_buffer, 0, NULL);
	message_buffer = calloc(num_chars, 6);
	if (w_message_buffer)
	{
		if (message_buffer)
		{
			WideCharToMultiByte(CP_UTF8, 0, w_message_buffer, num_chars, message_buffer, num_chars * 6, NULL, NULL);
			fprintf(stderr, "%s: %s\n", performing, message_buffer);
		}
		else
		{
			fprintf(stderr, "%s: %S\n", performing, w_message_buffer);
		}
	}
	LocalFree(w_message_buffer);
	free(message_buffer);
}

static void ShowLastError(const char *performing)
{
	ShowError(GetLastError(), performing);
}

static PhatBool_t VHDFileExists()
{
	DWORD attrib = GetFileAttributesW(BSP_DeviceFilePath);
	return (attrib != INVALID_FILE_ATTRIBUTES &&
		!(attrib & FILE_ATTRIBUTE_DIRECTORY));
}

static PhatBool_t GetPrivileged()
{
	HANDLE hToken = INVALID_HANDLE_VALUE;
	uint32_t privilege_buffer[sizeof(TOKEN_PRIVILEGES) + sizeof(LUID_AND_ATTRIBUTES) * 2] = { 0 };
	TOKEN_PRIVILEGES *tp = (TOKEN_PRIVILEGES *)privilege_buffer;
	tp->PrivilegeCount = 3;

	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
	{
		ShowLastError("Querying privileges");
		goto ErrRet;
	}

	if (!LookupPrivilegeValue(NULL, SE_MANAGE_VOLUME_NAME, &tp->Privileges[0].Luid))
	{
		ShowLastError("Looking up manage volume privilege LUID");
		goto ErrRet;
	}

	if (!LookupPrivilegeValue(NULL, SE_BACKUP_NAME, &tp->Privileges[1].Luid))
	{
		ShowLastError("Looking up backup privilege LUID");
		goto ErrRet;
	}

	if (!LookupPrivilegeValue(NULL, SE_RESTORE_NAME, &tp->Privileges[2].Luid))
	{
		ShowLastError("Looking up restore privilege LUID");
		goto ErrRet;
	}

	if (!AdjustTokenPrivileges(hToken, FALSE, tp, sizeof privilege_buffer, NULL, NULL))
	{
		ShowLastError("Adjust privileges");
		goto ErrRet;
	}

	CloseHandle(hToken);
	return 1;
ErrRet:
	if (hToken != INVALID_HANDLE_VALUE) CloseHandle(hToken);
	return 0;
}

static uint16_t BSwap16(uint16_t val)
{
	union {
		uint8_t u8s[2];
		uint16_t u16;
	}u1, u2;

	u1.u16 = val;
	u2.u8s[0] = u1.u8s[1];
	u2.u8s[1] = u1.u8s[0];
	return u2.u16 ;
}

static uint32_t BSwap32(uint32_t val)
{
	union {
		uint16_t u16s[2];
		uint32_t u32;
	}u1, u2;

	u1.u32 = val;
	u2.u16s[0] = BSwap16(u1.u16s[1]);
	u2.u16s[1] = BSwap16(u1.u16s[0]);
	return u2.u32;
}

static uint64_t BSwap64(uint64_t val)
{
	union {
		uint32_t u32s[2];
		uint64_t u64;
	}u1, u2;

	u1.u64 = val;
	u2.u32s[0] = BSwap32(u1.u32s[1]);
	u2.u32s[1] = BSwap32(u1.u32s[0]);
	return u2.u64;
}

static PhatBool_t CreateVHD(uint64_t size)
{
	VHD_Footer_t footer = { 0 };
	LARGE_INTEGER li;
	uint64_t total_sectors = size / 512;
	uint64_t cylinder_times_heads;
	uint16_t cylinders;
	uint32_t checksum = 0;
	uint8_t *footer_ptr = (uint8_t*)&footer;
	DWORD written = 0;

	hDevice = CreateFileW(BSP_DeviceFilePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL);
	if (hDevice == INVALID_HANDLE_VALUE)
	{
		ShowLastError("Create the VHD file as a disk image by using `CreateFileW()`");
		goto FailExit;
	}

	li.QuadPart = size;
	if (SetFilePointerEx(hDevice, li, NULL, FILE_BEGIN) == 0)
	{
		ShowLastError("Extending the VHD file to the target size by using `SetFilePointerEx()`");
		goto FailExit;
	}

	if (total_sectors >= 65535 * 16 * 63)
	{
		footer.sectors = 255;
		footer.heads = 16;
		cylinder_times_heads = total_sectors / footer.sectors;
		total_sectors = 65535 * 16 * 255;
	}
	else
	{
		footer.sectors = 17;
		cylinder_times_heads = total_sectors / footer.sectors;

		footer.heads = (uint8_t)((cylinder_times_heads + 1023) / 1024);

		if (footer.heads < 4)
		{
			footer.heads = 4;
		}
		if (cylinder_times_heads >= (footer.heads * 1024) || footer.heads > 16)
		{
			footer.sectors = 31;
			footer.heads = 16;
			cylinder_times_heads = total_sectors / footer.sectors;
		}
		if (cylinder_times_heads >= (footer.heads * 1024))
		{
			footer.sectors = 63;
			footer.heads = 16;
			cylinder_times_heads = total_sectors / footer.sectors;
		}
	}
	cylinders = (uint16_t)(cylinder_times_heads / footer.heads);

	memcpy(footer.cookie, "conectix", 8);
	footer.features = BSwap32(2);
	footer.format_version = BSwap32(0x00010000);
	footer.data_offset = 0xFFFFFFFFFFFFFFFFULL;
	footer.timestamp = BSwap32((uint32_t)(time(NULL) - 946684800));
	footer.creator_app = 0x74616850; // 'Phat'
	footer.creator_version = BSwap32(0x000A0000);
	footer.creator_host_os = BSwap32(0x5769326B);
	footer.original_size = BSwap64(size);
	footer.current_size = BSwap64(size);
	footer.cylinders = BSwap16(cylinders);
	footer.heads = 16;
	footer.sectors = 63;
	footer.disk_type = BSwap32(2);
	UuidCreateSequential(&footer.unique_id);
	footer.saved_state = 0;

	for (size_t i = 0; i < 512; i++)
	{
		checksum += footer_ptr[i];
	}
	footer.checksum = BSwap32(~checksum);

	if (!WriteFile(hDevice, &footer, 512, &written, NULL))
	{
		ShowLastError("Extending the VHD file to the target size by using `WriteFile()` to write the footer");
		goto FailExit;
	}
	if (written != 512)
	{
		fprintf(stderr, "`WriteFile()` write for 512 bytes, but %d byte(s) were actually written.\n", written);
		goto FailExit;
	}

	CloseHandle(hDevice);
	hDevice = INVALID_HANDLE_VALUE;
	return 1;
FailExit:
	if (hDevice != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hDevice);
		hDevice = INVALID_HANDLE_VALUE;
	}
	return 0;
}

static PhatBool_t MountVHD()
{
	WCHAR vhd_path[4096];
	const DWORD buffer_len = sizeof vhd_path / sizeof vhd_path[0];

	if (!VHDFileExists())
	{
		if (!CreateVHD(1024ull * 1024 * 1024 * 8))
		{
			fprintf(stderr, "Please create a VHD file (%S), initialize to MBR, create a FAT32 partition, then quick format the partition.\n", BSP_DeviceFilePath);
			return 0;
		}
	}

	DWORD length = GetFullPathNameW(BSP_DeviceFilePath, buffer_len, vhd_path, NULL);
	if (length == 0)
	{
		ShowLastError("Get VHD absolute path");
		return 0;
	}

	GetPrivileged();

	VIRTUAL_STORAGE_TYPE storageType = { 0 };
	storageType.DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_VHD;
	storageType.VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT;

	HANDLE vhd_handle = INVALID_HANDLE_VALUE;
	DWORD result = OpenVirtualDisk(
		&storageType,
		vhd_path,
		VIRTUAL_DISK_ACCESS_ATTACH_RW,
		OPEN_VIRTUAL_DISK_FLAG_NONE,
		NULL,
		&vhd_handle
	);

	if (result)
	{
		ShowError(result, "Opening VHD to mount");
		return 0;
	}

	result = AttachVirtualDisk(vhd_handle, NULL, ATTACH_VIRTUAL_DISK_FLAG_PERMANENT_LIFETIME, 0, NULL, NULL);
	if (result)
	{
		ShowError(result, "Mounting VHD");
		CloseHandle(vhd_handle);
		return 0;
	}
	CloseHandle(vhd_handle);

	return 1;
}

static PhatBool_t UnmountVHD()
{
	WCHAR vhd_path[4096];
	const DWORD buffer_len = sizeof vhd_path / sizeof vhd_path[0];
	DWORD length;

	if (!VHDFileExists())
	{
		if (!CreateVHD(1024ull * 1024 * 1024 * 8))
		{
			fprintf(stderr, "Please create a VHD file (%S), initialize to MBR, create a FAT32 partition, then quick format the partition.\n", BSP_DeviceFilePath);
			return 0;
		}
		return 1;
	}

	length = GetFullPathNameW(BSP_DeviceFilePath, buffer_len, vhd_path, NULL);
	if (length == 0)
	{
		ShowLastError("Get VHD absolute path");
		return 0;
	}

	GetPrivileged();

	VIRTUAL_STORAGE_TYPE storageType = { 0 };
	storageType.DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_VHD;
	storageType.VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT;

	HANDLE vhd_handle = INVALID_HANDLE_VALUE;
	DWORD result = OpenVirtualDisk(
		&storageType,
		vhd_path,
		VIRTUAL_DISK_ACCESS_DETACH,
		OPEN_VIRTUAL_DISK_FLAG_NONE,
		NULL,
		&vhd_handle
	);

	if (result)
	{
		ShowError(result, "Opening VHD to unmount");
		return 0;
	}

	result = DetachVirtualDisk(vhd_handle, ATTACH_VIRTUAL_DISK_FLAG_NONE, 0);
	if (result)
	{
		ShowError(result, "Unmounting VHD");
		CloseHandle(vhd_handle);
		return 0;
	}
	CloseHandle(vhd_handle);

	return 1;
}

__weak PhatBool_t BSP_OpenDevice(void *userdata)
{
	UNUSED(userdata);
	UnmountVHD();
	hDevice = CreateFileW(BSP_DeviceFilePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (hDevice == INVALID_HANDLE_VALUE)
	{
		ShowLastError("Open VHD file as a disk image by using `CreateFileW()`");
		return 0;
	}
	return 1;
}

__weak PhatBool_t BSP_CloseDevice(void *userdata)
{
	UNUSED(userdata);
	if (hDevice != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hDevice);
		hDevice = INVALID_HANDLE_VALUE;
	}
	MountVHD();
	return 1;
}

__weak PhatBool_t BSP_ReadSector(void *buffer, LBA_t LBA, size_t num_blocks, void *userdata)
{
	UNUSED(userdata);
	DWORD num_read = 0;
	LARGE_INTEGER Distance;
	if (hDevice == INVALID_HANDLE_VALUE)
	{
		fprintf(stderr, "Device not opened.\n");
		return 0;
	}
	Distance.QuadPart = (uint64_t)LBA * 512;
	if (!SetFilePointerEx(hDevice, Distance, NULL, FILE_BEGIN)) return 0;
	assert(num_blocks * 512 <= 0xFFFFFFFF);
	if (!ReadFile(hDevice, buffer, (DWORD)(num_blocks * 512), &num_read, NULL))
	{
		ShowLastError("Read sector");
		return 0;
	}
	if (num_read != num_blocks * 512) return 0;
	return 1;
}

__weak LBA_t BSP_GetDeviceCapacity(void *userdata)
{
	UNUSED(userdata);
	LARGE_INTEGER file_size;

	if (!GetFileSizeEx(hDevice, &file_size))
	{
		ShowLastError("Get the VHD capacity by using `GetFileSizeEx()`");
		return 0;
	}
	return (LBA_t)(file_size.QuadPart / 512) - 1;
}

__weak PhatBool_t BSP_WriteSector(const void *buffer, LBA_t LBA, size_t num_blocks, void *userdata)
{
	UNUSED(userdata);
	DWORD num_wrote = 0;
	LARGE_INTEGER Distance;
	if (hDevice == INVALID_HANDLE_VALUE)
	{
		fprintf(stderr, "Device not opened.\n");
		return 0;
	}
	Distance.QuadPart = (uint64_t)LBA * 512;
	if (!SetFilePointerEx(hDevice, Distance, NULL, FILE_BEGIN)) return 0;
	assert(num_blocks * 512 <= 0xFFFFFFFF);
	if (!WriteFile(hDevice, buffer, (DWORD)(num_blocks * 512), &num_wrote, NULL))
	{
		ShowLastError("Write sector");
		return 0;
	}
	if (num_wrote != num_blocks * 512) return 0;
	return 1;
}

#elif defined(__GNUC__) || (defined (__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050))
#ifndef __weak
#define __weak __attribute__((weak))
#endif

#include <string.h>

#include "stm32h7xx_hal.h"

extern SD_HandleTypeDef hsd1;
#if PHAT_USE_DMA
volatile int SD1_TxCplt;
volatile int SD1_RxCplt;
#ifndef PHAT_DMA_BUFFER
#define PHAT_DMA_BUFFER 4
#endif
#ifndef PHAT_IS_DMA_ALLOWED_ADDRESS
#define PHAT_IS_DMA_ALLOWED_ADDRESS(buffer, NB) (((size_t)(buffer) >= 0x08000000 && ((size_t)(buffer) + (NB) * 512) < 0x08020000) || ((size_t)(buffer) >= 0x24000000 && ((size_t)(buffer) + (NB) * 512) < 0x24080000))
#endif
uint8_t BSP_DMABuffer[PHAT_DMA_BUFFER * 512];
#endif

__weak PhatBool_t BSP_OpenDevice(void *userdata)
{
#ifndef DISABLE_SD_INIT
	static uint32_t init_clock_div;
	UNUSED(userdata);
	if (!init_clock_div)
	{
	  init_clock_div = hsd1.Init.ClockDiv;
	  if (!init_clock_div) init_clock_div = 1;
	}
	for (int i = 0; i < 5; i++)
	{
		__HAL_RCC_SDMMC1_FORCE_RESET();
		HAL_Delay(2);
		__HAL_RCC_SDMMC1_RELEASE_RESET();
		hsd1.Init.ClockDiv = init_clock_div + i;
		if (HAL_SD_Init(&hsd1) == HAL_OK) return 1;
	}
	return 0;
#else
  UNUSED(userdata);
	return 1;
#endif
}

__weak PhatBool_t BSP_CloseDevice(void *userdata)
{
	UNUSED(userdata);
#ifndef DISABLE_SD_INIT
	if (HAL_SD_DeInit(&hsd1) == HAL_OK) return 1;
	return 0;
#else
	return 1;
#endif
}

#if PHAT_USE_DMA
void HAL_SD_TxCpltCallback(SD_HandleTypeDef *hsd)
{
	if (hsd == &hsd1)
	{
		SD1_TxCplt = 1;
	}
}

void HAL_SD_RxCpltCallback(SD_HandleTypeDef *hsd)
{
	if (hsd == &hsd1)
	{
		SD1_RxCplt = 1;
	}
}
#endif

__weak PhatBool_t BSP_ReadSector(void *buffer, LBA_t LBA, size_t num_blocks, void *userdata)
{
#if PHAT_USE_DMA
	if ((size_t)buffer & 3 || !PHAT_IS_DMA_ALLOWED_ADDRESS(buffer, num_blocks))
	{
		//For unaligned address
		while(num_blocks)
		{
			size_t blocks_to_read = num_blocks;
			if (blocks_to_read > PHAT_DMA_BUFFER) blocks_to_read = PHAT_DMA_BUFFER;
			if (!BSP_ReadSector(BSP_DMABuffer, LBA, blocks_to_read, userdata)) return 0;
			memcpy(buffer, BSP_DMABuffer, blocks_to_read * 512);
			buffer = (uint8_t*)buffer + blocks_to_read * 512;
			num_blocks -= blocks_to_read;
			LBA += blocks_to_read;
		}
		return 1;
	}
	else
	{
		//For aligned address
		uint32_t timeout = HAL_GetTick() + SDMMC_SWDATATIMEOUT;
		SD1_RxCplt = 0;
		SCB_CleanDCache_by_Addr((uint32_t*)buffer, num_blocks * 512);
		if (HAL_SD_ReadBlocks_DMA(&hsd1, (uint8_t *)buffer, LBA, num_blocks) == HAL_OK)
		{
			for (;;)
			{
				if (SD1_RxCplt)
				{
					SCB_InvalidateDCache_by_Addr(buffer, num_blocks * 512);
					return 1;
				}
				if (HAL_GetTick() <= timeout) __WFI();
				else return 0;
			}
		}
	}
#else
	UNUSED(userdata);
	if (HAL_SD_ReadBlocks(&hsd1, (uint8_t *)buffer, LBA, num_blocks, SDMMC_SWDATATIMEOUT) == HAL_OK) return 1;
#endif
	return 0;
}

__weak PhatBool_t BSP_WriteSector(const void *buffer, LBA_t LBA, size_t num_blocks, void *userdata)
{
#if PHAT_USE_DMA
	if ((size_t)buffer & 3 || !PHAT_IS_DMA_ALLOWED_ADDRESS(LBA, num_blocks))
	{
		//For unaligned address
		while(num_blocks)
		{
			size_t blocks_to_read = num_blocks;
			if (blocks_to_read > PHAT_DMA_BUFFER) blocks_to_read = PHAT_DMA_BUFFER;
			memcpy(BSP_DMABuffer, buffer, blocks_to_read * 512);
			if (!BSP_WriteSector(BSP_DMABuffer, LBA, blocks_to_read, userdata)) return 0;
			buffer = (uint8_t*)buffer + blocks_to_read * 512;
			num_blocks -= blocks_to_read;
			LBA += blocks_to_read;
		}
		return 1;
	}
	else
	{
		//For aligned address
		uint32_t timeout = HAL_GetTick() + SDMMC_SWDATATIMEOUT;
		SD1_TxCplt = 0;
		SCB_CleanDCache_by_Addr((uint32_t*)buffer, num_blocks * 512);
		if (HAL_SD_WriteBlocks_DMA(&hsd1, (const uint8_t *)buffer, LBA, num_blocks) == HAL_OK)
		{
			for (;;)
			{
				if (SD1_TxCplt) return 1;
				if (HAL_GetTick() <= timeout)__WFI();
				else return 0;
			}
		}
	}
#else
	UNUSED(userdata);
	if (HAL_SD_WriteBlocks(&hsd1, (const uint8_t *)buffer, LBA, num_blocks, SDMMC_SWDATATIMEOUT) == HAL_OK) return 1;
#endif
	return 0;
}

__weak LBA_t BSP_GetDeviceCapacity(void *userdata)
{
	UNUSED(userdata);
	HAL_SD_CardInfoTypeDef info;
	if (HAL_SD_GetCardInfo(&hsd1, &info) != HAL_OK) return 0;
	return info.BlockNbr;
}
#endif

__weak Phat_Disk_Driver_t Phat_InitDriver(void *userdata)
{
	Phat_Disk_Driver_t ret = { 0 };
	ret.userdata = userdata;
	ret.fn_open_device = BSP_OpenDevice;
	ret.fn_read_sector = BSP_ReadSector;
	ret.fn_write_sector = BSP_WriteSector;
	ret.fn_close_device = BSP_CloseDevice;
	return ret;
}

__weak void Phat_DeInitDriver(Phat_Disk_Driver_p driver)
{
	memset(driver, 0, sizeof * driver);
}

__weak PhatBool_t Phat_OpenDevice(Phat_Disk_Driver_p driver)
{
	if (driver->fn_open_device(driver->userdata))
	{
		driver->device_capacity_in_sectors = BSP_GetDeviceCapacity(driver->userdata);
		if (!driver->device_capacity_in_sectors)
		{
			driver->fn_close_device(driver->userdata);
			return 0;
		}
		driver->device_opended = 1;
		return 1;
	}
	return 0;
}

__weak PhatBool_t Phat_CloseDevice(Phat_Disk_Driver_p driver)
{
	return driver->fn_close_device(driver->userdata);
}

__weak PhatBool_t Phat_ReadSector(Phat_Disk_Driver_p driver, void *buffer, LBA_t LBA, size_t num_blocks)
{
	return driver->fn_read_sector(buffer, LBA, num_blocks, driver->userdata);
}

__weak PhatBool_t Phat_WriteSector(Phat_Disk_Driver_p driver, const void *buffer, LBA_t LBA, size_t num_blocks)
{
	return driver->fn_write_sector(buffer, LBA, num_blocks, driver->userdata);
}
