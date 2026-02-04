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

struct lkpi_cdevtest_poll {
	uint32_t flags;
};

#define LKPI_CDEVTEST_POLL_READ 0x00000001U
#define LKPI_CDEVTEST_POLL_WRITE 0x00000002U

#define LKPI_CDEVTEST_IOC_MAGIC 'L'
#define LKPI_CDEVTEST_IOC_SET _IOW(LKPI_CDEVTEST_IOC_MAGIC, 1, \
	struct lkpi_cdevtest_ioctl)
#define LKPI_CDEVTEST_IOC_GET _IOR(LKPI_CDEVTEST_IOC_MAGIC, 2, \
	struct lkpi_cdevtest_ioctl)
#define LKPI_CDEVTEST_IOC_POLL_SET _IOW(LKPI_CDEVTEST_IOC_MAGIC, 3, \
	struct lkpi_cdevtest_poll)
#define LKPI_CDEVTEST_IOC_POLL_CLR _IOW(LKPI_CDEVTEST_IOC_MAGIC, 4, \
	struct lkpi_cdevtest_poll)

#endif
