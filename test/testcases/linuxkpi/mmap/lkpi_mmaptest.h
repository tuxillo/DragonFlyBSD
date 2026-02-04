#ifndef LKPI_MMAPTEST_H
#define LKPI_MMAPTEST_H

#include <sys/ioccom.h>
#include <stdint.h>

#define LKPI_MMAPTEST_DEVNAME "lkpi_mmaptest"
#define LKPI_MMAPTEST_MODULE "lkpi_mmaptest_kmod"
#define LKPI_MMAPTEST_KO_PATH "kmod/lkpi_mmaptest_kmod.ko"

#define LKPI_MMAPTEST_IOC_MAGIC 'M'

enum lkpi_mmap_cache {
	LKPI_MMAP_CACHE_WB = 0,
	LKPI_MMAP_CACHE_WC = 1,
	LKPI_MMAP_CACHE_UC = 2,
};

struct lkpi_mmap_alloc {
	uint64_t size;
	uint32_t cache;
	uint32_t flags;
	uint64_t mmap_off;
	uint64_t handle;
};

struct lkpi_mmap_free {
	uint64_t handle;
};

struct lkpi_mmap_query {
	uint64_t handle;
	uint64_t size;
	uint32_t cache;
	uint32_t flags;
	uint64_t mmap_off;
};

struct lkpi_mmap_memattr {
	uint64_t handle;
	uint32_t memattr;
	uint32_t flags;
};

#define LKPI_MMAPTEST_ALLOC _IOWR(LKPI_MMAPTEST_IOC_MAGIC, 0x01, \
	struct lkpi_mmap_alloc)
#define LKPI_MMAPTEST_FREE _IOW(LKPI_MMAPTEST_IOC_MAGIC, 0x02, \
	struct lkpi_mmap_free)
#define LKPI_MMAPTEST_QUERY _IOWR(LKPI_MMAPTEST_IOC_MAGIC, 0x03, \
	struct lkpi_mmap_query)
#define LKPI_MMAPTEST_GET_MEMATTR _IOWR(LKPI_MMAPTEST_IOC_MAGIC, 0x04, \
	struct lkpi_mmap_memattr)

#endif
