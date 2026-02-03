#ifndef LKPI_CDEVTEST_H
#define LKPI_CDEVTEST_H

#include <sys/ioccom.h>
#include <stdint.h>

#define LKPI_CDEVTEST_DEVNAME "lkpi_cdevtest"
#define LKPI_CDEVTEST_MODULE "lkpi_cdevtest_kmod"
#define LKPI_CDEVTEST_KO_PATH "kmod/lkpi_cdevtest_kmod.ko"

struct lkpi_cdevtest_ioctl {
	uint32_t value;
};

#define LKPI_CDEVTEST_IOC_MAGIC 'L'
#define LKPI_CDEVTEST_IOC_SET _IOW(LKPI_CDEVTEST_IOC_MAGIC, 1, \
	struct lkpi_cdevtest_ioctl)
#define LKPI_CDEVTEST_IOC_GET _IOR(LKPI_CDEVTEST_IOC_MAGIC, 2, \
	struct lkpi_cdevtest_ioctl)

#endif
