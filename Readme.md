# Phat: FAT filesystem API

## Overview

Phat is a FAT filesystem API designed for embedded systems and cross-platform development.

* No dynamic memory allocation is used.
* Filenames and directories are encoded in UTF-16.
* The default code page is 437 (OEM United States).
* Includes an LRU (Least Recently Used) sector cache.

## Usage

First, review `BSP_phat.c` and `BSP_phat.h`. These files provide the low-level driver implementation that allows the PHAT API to access the storage device.

You need to implement the following functions:

```C
typedef int PhatBool_t;
PhatBool_t BSP_OpenDevice(void *userdata);
PhatBool_t BSP_CloseDevice(void *userdata);
PhatBool_t BSP_ReadSector(void *buffer, LBA_t LBA, size_t num_blocks, void *userdata);
PhatBool_t BSP_WriteSector(void *buffer, LBA_t LBA, size_t num_blocks, void *userdata);
```

A default implementation is provided for the STM32H750 microcontroller to read/write via SDMMC1 when compiling with GCC or ARMCC.

If `_WIN32` is defined, the default implementation uses `CreateFileW()` to open `\\.\PhysicalDrive3`. This means the fourth disk drive in your Windows system will be used for debugging. Be cautious when running the code on Windows, as it will access a physical drive.

All you need is `BSP_phat.c`, `BSP_phat.h`, `phat.c`, `phat.h`.

## Example: Iterate through a directory

```C
#include <stdio.h>
#include <stdlib.h>

#include "phat.h"

Phat_t phat;

void Error_Handler()
{
	exit(1);
}

#define V(x) if (x != PhatState_OK) Error_Handler()
#define V_(x) do {PhatState s = x; if (s != PhatState_OK) fprintf(stderr, #x ": %s\n", Phat_StateToString(s));} while (0)

int main(int argc, char**argv)
{
	PhatState res = PhatState_OK;
	Phat_DirInfo_t dir_info = { 0 };
	Phat_FileInfo_t file_info = { 0 };
	uint32_t file_size;
	char *file_buf = NULL;

	V(Phat_Init(&phat));
	V(Phat_Mount(&phat, 0));

	printf("==== Root directory files ====\n");
	V(Phat_OpenDir(&phat, L"", &dir_info));
	for (;;)
	{
		res = Phat_NextDirItem(&dir_info);
		if (res != PhatState_OK) break;
		if (dir_info.attributes & ATTRIB_DIRECTORY)
			printf("Dir:  %S\n", dir_info.LFN_name);
		else
			printf("File: %S\n", dir_info.LFN_name);
	}
	Phat_CloseDir(&dir_info);

	printf("==== Files in `TestPhat` directory ====\n");
	V_(Phat_OpenDir(&phat, L"TestPhat", &dir_info));
	for (;;)
	{
		res = Phat_NextDirItem(&dir_info);
		if (res != PhatState_OK) break;
		if (dir_info.attributes & ATTRIB_DIRECTORY)
			printf("Dir:  %S\n", dir_info.LFN_name);
		else
			printf("File: %S\n", dir_info.LFN_name);
	}
	Phat_CloseDir(&dir_info);

	V_(Phat_CreateDirectory(&phat, L"TestPhatMkDir"));
	V_(Phat_RemoveDirectory(&phat, L"TestPhatMkDir"));

	V_(Phat_OpenFile(&phat, L"TestPhat/The Biography of John Wok.txt", 1, &file_info));
	Phat_GetFileSize(&file_info, &file_size);
	file_buf = calloc(file_size + 1, 1);
	if (!file_buf) goto FailExit;
	V_(Phat_ReadFile(&file_info, file_buf, file_size, NULL));
	Phat_CloseFile(&file_info);
	printf("File contents:\n%s\n", file_buf);
	free(file_buf);

FailExit:
	V_(Phat_Unmount(&phat));
	V_(Phat_DeInit(&phat));
	return 0;
}
```

## Currently Supported

* Basic implementation for FAT12/16/32.
	* Iterate through directory
	* Open file(with creation or readonly)
	* Read file
	* Write file
	* Seek file
	* Delete file
	* Create directory
	* Remove directory
* Debugging on Windows by accessing a virtual drive.
* Support for multiple partitions.
