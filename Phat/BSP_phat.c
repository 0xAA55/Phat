#include "BSP_Phat.h"

#ifdef _WIN32
#ifndef __weak
#define __weak
#endif
#endif

__weak PhatBool_t BSP_OpenDevice(void *userdata);
__weak PhatBool_t BSP_CloseDevice(void *userdata);
__weak PhatBool_t BSP_ReadSector(void *buffer, LBA_t LBA, size_t num_blocks, void *userdata);
__weak PhatBool_t BSP_WriteSector(void *buffer, LBA_t LBA, size_t num_blocks, void *userdata);

#ifdef _WIN32
#define INITGUID
#include <stdio.h>
#include <assert.h>
#include <Windows.h>
#include <virtdisk.h>
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "VirtDisk.lib")

static const WCHAR* BSP_DeviceFilePath = L"test16.vhd";

static HANDLE hDevice = INVALID_HANDLE_VALUE;

static void ShowError(DWORD error_code, const char *performing)
{
	LPWSTR message_buffer = NULL;

	//Ask Win32 to give us the string version of that message ID.
	//The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
	size_t size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&message_buffer, 0, NULL);
	fprintf(stderr, "%s: %S\n", performing, message_buffer);
	LocalFree(message_buffer);
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

static PhatBool_t MountVHD()
{
	WCHAR vhd_path[4096];
	const DWORD buffer_len = sizeof vhd_path / sizeof vhd_path[0];

	if (!VHDFileExists())
	{
		fprintf(stderr, "Please create a VHD file (%S), initialize to MBR, create a FAT32 partition, then quick format the partition.\n", BSP_DeviceFilePath);
		return 0;
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
	UnmountVHD();
	hDevice = CreateFileW(BSP_DeviceFilePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (hDevice == INVALID_HANDLE_VALUE)
	{
		ShowLastError("CreateFileW()");
		return 0;
	}
	return 1;
}

__weak PhatBool_t BSP_CloseDevice(void *userdata)
{
	if (hDevice != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hDevice);
		hDevice = INVALID_HANDLE_VALUE;
	}
	return MountVHD();
}

__weak PhatBool_t BSP_ReadSector(void *buffer, LBA_t LBA, size_t num_blocks, void *userdata)
{
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

__weak PhatBool_t BSP_WriteSector(const void *buffer, LBA_t LBA, size_t num_blocks, void *userdata)
{
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

__weak PhatBool_t BSP_OpenDevice(void *userdata)
{
#ifndef DISABLE_SD_INIT
	if (HAL_SD_Init(&hsd1) == HAL_OK) return 1;
	return 0;
#else
	return 1;
#endif
}

__weak PhatBool_t BSP_CloseDevice(void *userdata)
{
#ifndef DISABLE_SD_INIT
	if (HAL_SD_DeInit(&hsd1) == HAL_OK) return 1;
	return 0;
#else
	return 1;
#endif
}

__weak PhatBool_t BSP_ReadSector(void *buffer, LBA_t LBA, size_t num_blocks, void *userdata)
{
	if (HAL_SD_ReadBlocks(&hsd1, (uint8_t *)buffer, LBA, num_blocks, SDMMC_SWDATATIMEOUT) == HAL_OK) return 1;
	return 0;
}

__weak PhatBool_t BSP_WriteSector(const void *buffer, LBA_t LBA, size_t num_blocks, void *userdata)
{
	if (HAL_SD_WriteBlocks(&hsd1, (const uint8_t *)buffer, LBA, num_blocks, SDMMC_SWDATATIMEOUT) == HAL_OK) return 1;
	return 0;
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
	return driver->fn_open_device(driver->userdata);
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
