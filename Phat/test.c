
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
