# Phat: FAT 文件系统 API

## 语言 Language

简体中文 | [Chinglish](Readme.md)

## 概述

Phat 是一个专为嵌入式系统和跨平台开发设计的 FAT 文件系统 API。
* 支持 MBR、GPT 分区表
* 支持无分区表（使整个磁盘为一个分区）
* 支持 FAT12/16/32 的基本实现。
	* 遍历目录
	* 打开文件（可创建或只读）
	* 读取文件
	* 写入文件
	* 文件寻址
	* 删除文件
	* 创建目录
	* 删除目录
	* 重命名
	* 移动
	* 将磁盘初始化为 MBR/GPT
	* 在 MBR/GPT 磁盘上创建分区
	* MakeFS: 将分区格式化为 FAT12/16/32
* 在 Windows 上通过虚拟磁盘功能进行调试。
* 支持多分区。
* 不使用动态内存分配（Windows 调试代码除外）。
* 文件名和目录采用 UTF-16 编码。
* 默认代码页为 437（OEM 美国）。
* 包含 LRU（最近最少使用）扇区缓存。
* 对任何路径长度没有限制（仅限制文件名/目录名长度 ≤ 255）。

## 用法

首先，你需要查看 `BSP_phat.c` 和 `BSP_phat.h` 文件。这些文件提供了底层驱动实现，使 PHAT API 能够访问存储设备。

你需要实现以下几个函数：

```C
typedef int PhatBool_t;
PhatBool_t BSP_OpenDevice(void *userdata);
PhatBool_t BSP_CloseDevice(void *userdata);
PhatBool_t BSP_ReadSector(void *buffer, LBA_t LBA, size_t num_blocks, void *userdata);
PhatBool_t BSP_WriteSector(void *buffer, LBA_t LBA, size_t num_blocks, void *userdata);
LBA_t BSP_GetDeviceCapacity(void *userdata);
```

为 STM32H750 微控制器提供了默认实现，在使用 GCC 或 ARMCC 编译时可通过 SDMMC1 进行读写操作。

如果定义了 `_WIN32`，默认实现会使用 `CreateFileW()` 打开 `test.vhd`。

虚拟硬盘会被自动创建，测试代码退出后会自动被挂载到系统，也就是你会发现你多了一个硬盘。查看这个硬盘可以观察文件系统是否正常。

你只需要 `BSP_phat.c`、`BSP_phat.h`、`phat.c` 和 `phat.h` 这几个文件。将它们添加到您的项目中即可。

## 例子代码

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
