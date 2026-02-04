#ifndef LKPI_SGTEST_H
#define LKPI_SGTEST_H

#include <sys/ioccom.h>
#include <stdint.h>

#define LKPI_SGTEST_DEVNAME "lkpi_sgtest"
#define LKPI_SGTEST_MODULE "lkpi_sgtest_kmod"
#define LKPI_SGTEST_KO_PATH "kmod/lkpi_sgtest_kmod.ko"

#define LKPI_SGTEST_IOC_MAGIC 'S'

enum lkpi_sgtest_subtest {
	LKPI_SGTEST_SUBTEST_HIGHMEM = 1,
	LKPI_SGTEST_SUBTEST_SG_TABLE = 2,
	LKPI_SGTEST_SUBTEST_SG_COPY = 3,
	LKPI_SGTEST_SUBTEST_SG_CHAIN = 4,
};

struct lkpi_sgtest_result {
	uint32_t status;
	uint32_t subtest;
	int32_t err;
	uint32_t flags;
};

#define LKPI_SGTEST_RUN _IOWR(LKPI_SGTEST_IOC_MAGIC, 0x01, \
	struct lkpi_sgtest_result)

#endif
