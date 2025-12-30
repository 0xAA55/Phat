# Phat: FAT filesystem API

## Overview

Phat is a FAT filesystem API designed for embedded systems and cross-platform development.

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

## Example: Iterate through a directory

```C
#include <stdio.h>

#include "phat.h"

Phat_t phat;

void Error_Handler()
{
	for (;;);
}

#define V(x) if (x != PhatState_OK) Error_Handler()

int main()
{
	Phat_DirInfo_t dir_info;
	WChar_t path[256] = L"";
	PhatState res = PhatState_OK;
	V(Phat_Init(&phat));

	V(Phat_Mount(&phat, 0));

	V(Phat_OpenDir(&phat, path, &dir_info));
	for (;;)
	{
		res = Phat_NextDirItem(&phat, &dir_info);
		if (res != PhatState_OK) break;
		if (dir_info.attributes & ATTRIB_DIRECTORY)
			printf("Dir:  %S\n", dir_info.LFN_name);
		else
			printf("File: %S\n", dir_info.LFN_name);
	}
	V(Phat_CloseDir(&phat, &dir_info));

	V(Phat_DeInit(&phat));
	return 0;
}
```

## Currently Supported

* Basic read‑only implementation for FAT32.
* Debugging on Windows by accessing a physical drive.
* Support for multiple partitions.

## TODO

* Read‑write implementation.
* Testing for FAT12/FAT16.
