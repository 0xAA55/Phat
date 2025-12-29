
#include <stdio.h>

#include "phat.h"

Phat_t Phat;

void Error_Handler()
{
	for (;;);
}

#define V(x) if (x != PhatState_OK) Error_Handler()

int main()
{
	V(Phat_Init(&Phat));

	V(Phat_Mount(&Phat, 0));


	V(Phat_DeInit(&Phat));
	return 0;
}
