
#include <stdio.h>
#include <wchar.h>

#include "phat.h"

Phat_t phat;

void Error_Handler()
{
	exit(1);
}

#define V(x) if (x != PhatState_OK) Error_Handler()

int main(int argc, char**argv)
{
	Phat_DirInfo_t dir_info;
	WChar_t path[256] = L"";
	PhatState res = PhatState_OK;

	V(Phat_Init(&phat));
	V(Phat_Mount(&phat, 0));

	V(Phat_OpenDir(&phat, path, &dir_info));
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

	wcscpy(path, L"TestPhat");
	V(Phat_OpenDir(&phat, path, &dir_info));
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

	wcscpy(path, L"TestPhatMkDir");
	V(Phat_CreateDirectory(&phat, path));

	V(Phat_Unmount(&phat));
	V(Phat_DeInit(&phat));
	return 0;
}
