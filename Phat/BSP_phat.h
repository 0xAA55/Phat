#ifndef _BSP_PHAT_
#define _BSP_PHAT_ 1

#include<stdint.h>

#ifdef PHAT_BIGLBA
typedef uint64_t LBA_t;
#else
typedef uint32_t LBA_t;
#endif

#ifndef SDMMC_SWDATATIMEOUT
#define SDMMC_SWDATATIMEOUT 200
#endif

#ifndef SDMMC_DATATIMEOUT
#define SDMMC_DATATIMEOUT 5000000
#endif

typedef int PhatBool_t;

typedef PhatBool_t(*FnOpenDevice)(void *userdata);
typedef PhatBool_t(*FnReadSector)(void *buffer, LBA_t LBA, size_t num_blocks, void *userdata);
typedef PhatBool_t(*FnWriteSector)(const void *buffer, LBA_t LBA, size_t num_blocks, void *userdata);
typedef PhatBool_t(*FnCloseDevice)(void *userdata);

typedef struct Phat_Disk_Driver_s
{
	void *userdata;
	FnOpenDevice fn_open_device;
	FnReadSector fn_read_sector;
	FnWriteSector fn_write_sector;
	FnCloseDevice fn_close_device;
	PhatBool_t device_opended;
}Phat_Disk_Driver_t, *Phat_Disk_Driver_p;

Phat_Disk_Driver_t Phat_InitDriver(void *userdata);
void Phat_DeInitDriver(Phat_Disk_Driver_p driver);

PhatBool_t Phat_OpenDevice(Phat_Disk_Driver_p driver);
PhatBool_t Phat_CloseDevice(Phat_Disk_Driver_p driver);

PhatBool_t Phat_ReadSector(Phat_Disk_Driver_p driver, void *buffer, LBA_t LBA, size_t num_blocks);
PhatBool_t Phat_WriteSector(Phat_Disk_Driver_p driver, const void *buffer, LBA_t LBA, size_t num_blocks);

#endif
