#ifndef LKPI_VM_RADIX_H
#define LKPI_VM_RADIX_H

#include <sys/ioccom.h>
#include <stdint.h>

#define LKPI_VMRADIX_DEVNAME "lkpi_vmradix"
#define LKPI_VMRADIX_MODULE "lkpi_vm_radixtest_kmod"
#define LKPI_VMRADIX_KO_PATH "kmod/lkpi_vm_radixtest_kmod.ko"

#define LKPI_VMRADIX_IOC_MAGIC 'R'

enum lkpi_vmradix_subtest {
	LKPI_VMRADIX_SUBTEST_FOREACH = 1,
	LKPI_VMRADIX_SUBTEST_FORALL_HOLES = 2,
	LKPI_VMRADIX_SUBTEST_FORALL_DENSE = 3,
	LKPI_VMRADIX_SUBTEST_LOOKUP = 4,
	LKPI_VMRADIX_SUBTEST_STEP_PREV = 5,
	LKPI_VMRADIX_SUBTEST_LIMIT = 6
};

struct lkpi_vmradix_result {
	uint32_t status;
	uint32_t subtest;
	int32_t err;
	uint32_t flags;
};

#define LKPI_VMRADIX_RUN _IOWR(LKPI_VMRADIX_IOC_MAGIC, 0x01, \
	struct lkpi_vmradix_result)

#endif
