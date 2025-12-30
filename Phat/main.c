
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
