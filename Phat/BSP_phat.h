#ifndef _BSP_PHAT_
#define _BSP_PHAT_ 1

#include<stdint.h>
#include<stddef.h>

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

#ifdef PHAT_BIGLBA
typedef uint64_t LBA_t, *LBA_p;
typedef int64_t SLBA_t, *SLBA_p;
#else
typedef uint32_t LBA_t, *LBA_p;
typedef int32_t SLBA_t, *SLBA_p;
#endif

#ifndef PHAT_FUNC
#define PHAT_FUNC
#endif

#ifndef SDMMC_SWDATATIMEOUT
#define SDMMC_SWDATATIMEOUT 200
#endif

#ifndef SDMMC_DATATIMEOUT
#define SDMMC_DATATIMEOUT 5000000
#endif

#ifndef PHAT_NO_DMA
#define PHAT_USE_DMA 1
#endif

typedef uint8_t PhatBool_t, *PhatBool_p;

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
	LBA_t device_capacity_in_sectors;
}Phat_Disk_Driver_t, *Phat_Disk_Driver_p;

PHAT_FUNC Phat_Disk_Driver_t Phat_InitDriver(void *userdata);
PHAT_FUNC void Phat_DeInitDriver(Phat_Disk_Driver_p driver);

PHAT_FUNC PhatBool_t Phat_OpenDevice(Phat_Disk_Driver_p driver);
PHAT_FUNC PhatBool_t Phat_CloseDevice(Phat_Disk_Driver_p driver);

PHAT_FUNC PhatBool_t Phat_ReadSector(Phat_Disk_Driver_p driver, void *buffer, LBA_t LBA, size_t num_blocks);
PHAT_FUNC PhatBool_t Phat_WriteSector(Phat_Disk_Driver_p driver, const void *buffer, LBA_t LBA, size_t num_blocks);

#ifdef _MSC_VER
#define PHAT_ALIGNMENT
#endif

#endif
