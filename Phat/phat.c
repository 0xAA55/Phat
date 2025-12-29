#include "phat.h"

#include <string.h>

static PhatBool_t Phat_IsCachedSectorSync(Phat_SectorCache_p cached_sector)
{
	return (cached_sector->usage & SECTORCACHE_SYNC) == SECTORCACHE_SYNC;
}

static void Phat_SetCachedSectorUnsync(Phat_SectorCache_p cached_sector)
{
	cached_sector->usage &= ~SECTORCACHE_SYNC;
}

static void Phat_SetCachedSectorSync(Phat_SectorCache_p cached_sector)
{
	cached_sector->usage |= SECTORCACHE_SYNC;
}

static uint32_t Phat_GetCachedSectorAge(Phat_SectorCache_p cached_sector)
{
	return cached_sector->usage & SECTORCACHE_AGE_BM;
}

static void Phat_SetCachedSectorAge(Phat_SectorCache_p cached_sector, uint32_t age)
{
	cached_sector->usage &= ~SECTORCACHE_AGE_BM;
	cached_sector->usage |= (age & SECTORCACHE_AGE_BM);
}

static PhatBool_t Phat_IsCachedSectorValid(Phat_SectorCache_p cached_sector)
{
	return (cached_sector->usage & SECTORCACHE_VALID) == SECTORCACHE_VALID;
}

static void Phat_SetCachedSectorValid(Phat_SectorCache_p cached_sector)
{
	cached_sector->usage |= SECTORCACHE_VALID;
}

static PhatBool_t Phat_InvalidateCachedSector(Phat_p phat, Phat_SectorCache_p cached_sector)
{
	if (Phat_IsCachedSectorValid(cached_sector) && !Phat_IsCachedSectorSync(cached_sector))
	{
		if (!Phat_WriteBackCachedSector(phat, cached_sector))
		{
			return 0;
		}
		Phat_SetCachedSectorSync(cached_sector);
	}
	cached_sector->usage &= ~SECTORCACHE_VALID;
}

static PhatBool_t Phat_WriteBackCachedSector(Phat_p phat, Phat_SectorCache_p cached_sector)
{
	if (!Phat_IsCachedSectorSync(cached_sector))
	{
		if (!phat->driver.fn_write_sector(cached_sector->data, cached_sector->LBA, 1, phat->driver.userdata))
		{
			return 0;
		}
		cached_sector->usage |= SECTORCACHE_SYNC;
	}
	return 1;
}

static PhatBool_t Phat_GetCachedSector(Phat_p phat, LBA_t LBA, Phat_SectorCache_p *pp_cached_sector)
{
	for (size_t i = 0; i < PHAT_CACHED_SECTORS; i++)
	{
		Phat_SectorCache_p cached_sector = &phat->cache[i];
		if (Phat_IsCachedSectorValid(cached_sector) && phat->cache[i].LBA == LBA)
		{
			Phat_SetCachedSectorUnsync(cached_sector);
			*pp_cached_sector = cached_sector;
			return 1;
		}
	}

	for (size_t i = 0; i < PHAT_CACHED_SECTORS; i++)
	{
		Phat_SectorCache_p cached_sector = &phat->cache[i];
		int MustDo = (i == PHAT_CACHED_SECTORS - 1);
		if (MustDo || (!Phat_IsCachedSectorValid(cached_sector) && Phat_GetCachedSectorAge(cached_sector) - phat->LRU_age >= PHAT_CACHED_SECTORS))
		{
			if (!Phat_InvalidateCachedSector(phat, cached_sector))
			{
				return 0;
			}
			if (!phat->driver.fn_read_sector(cached_sector->data, LBA, 1, phat->driver.userdata))
			{
				return 0;
			}
			phat->LRU_age++;
			cached_sector->LBA = LBA;
			Phat_SetCachedSectorValid(cached_sector);
			Phat_SetCachedSectorUnsync(cached_sector);
			Phat_SetCachedSectorAge(cached_sector, phat->LRU_age);
			*pp_cached_sector = cached_sector;
			return 1;
		}
	}
}

PhatBool_t Phat_Init(Phat_p phat)
{
	memset(phat, 0, sizeof * phat);
	phat->driver = Phat_InitDriver(NULL);
	return 1;
}

PhatBool_t Phat_DeInit(Phat_p phat)
{
	for (size_t i = 0; i < PHAT_CACHED_SECTORS; i++)
	{
		Phat_SectorCache_p cached_sector = &phat->cache[i];
		if (Phat_IsCachedSectorValid(cached_sector))
		{
			if (!Phat_InvalidateCachedSector(phat, cached_sector))
			{
				return 0;
			}
		}
	}
	Phat_DeInitDriver(&phat->driver);
	memset(phat, 0, sizeof * phat);
	return 1;
}

