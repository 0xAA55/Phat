#include "BSP_Phat.h"

#ifdef _WIN32
#ifndef __weak
#define __weak __declspec(selectany)
#endif

#include <Windows.h>
#include <stdio.h>

static const WCHAR* BSP_DeviceFilePath = L"\\\\.\\PhysicalDrive3:";

static HANDLE hDevice = INVALID_HANDLE_VALUE;

static void ShowLastError(const char* performing)
{
	DWORD last_error = GetLastError();
	LPSTR message_buffer = NULL;

	//Ask Win32 to give us the string version of that message ID.
	//The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
	size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, last_error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&message_buffer, 0, NULL);
	fprintf(stderr, "%s: %s\n", performing, message_buffer);
	LocalFree(message_buffer);
}

__weak PhatBool_t BSP_OpenDevice(void *userdata)
{
	BSP_CloseDevice(userdata);
	hDevice = CreateFileW(BSP_DeviceFilePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
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
	if (!ReadFile(hDevice, buffer, num_blocks * 512, &num_read, NULL)) return 0;
	if (num_read != num_blocks * 512) return 0;
	return 1;
}

__weak PhatBool_t BSP_WriteSector(void *buffer, LBA_t LBA, size_t num_blocks, void *userdata)
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
	if (!WriteFile(hDevice, buffer, num_blocks * 512, &num_wrote, NULL)) return 0;
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
