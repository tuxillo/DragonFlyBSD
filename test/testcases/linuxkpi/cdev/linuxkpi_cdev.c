#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
int kldload(const char *);
int kldunload(int);
int kldfind(const char *);
#include <sys/ioctl.h>

#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lkpi_cdevtest.h"

#define LKPI_CDEVTEST_DEVPATH "/dev/" LKPI_CDEVTEST_DEVNAME

static int
wait_for_device(const char *path, int retries, int usec)
{
	struct stat sb;

	while (retries-- > 0) {
		if (stat(path, &sb) == 0)
			return 0;
		usleep(usec);
	}

	return -1;
}

static const char *
find_kmod_path(void)
{
	static const char *candidates[] = {
		LKPI_CDEVTEST_KO_PATH,
		"kmod/obj/lkpi_cdevtest_kmod.ko",
		NULL
	};
	struct stat sb;
	int i;

	for (i = 0; candidates[i] != NULL; i++) {
		if (stat(candidates[i], &sb) == 0)
			return candidates[i];
	}

	return NULL;
}

static int
find_module_id(void)
{
	const char *candidates[] = {
		LKPI_CDEVTEST_KO_PATH,
		LKPI_CDEVTEST_MODULE ".ko",
		LKPI_CDEVTEST_MODULE,
		NULL
	};
	int i;
	int id;

	for (i = 0; candidates[i] != NULL; i++) {
		id = kldfind(candidates[i]);
		if (id >= 0)
			return id;
	}

	return -1;
}

static int
do_pre(void)
{
	const char *kmod_path;
	int id;

	kmod_path = find_kmod_path();
	if (kmod_path == NULL)
		errx(1, "kernel module not found under kmod/");

	printf("Loading %s...\n", kmod_path);
	id = kldload(kmod_path);
	if (id < 0)
		err(1, "kldload(%s)", kmod_path);

	if (wait_for_device(LKPI_CDEVTEST_DEVPATH, 200, 10000) != 0)
		errx(1, "device node %s not found", LKPI_CDEVTEST_DEVPATH);

	return 0;
}

static int
do_post(void)
{
	int id;

	id = find_module_id();
	if (id < 0)
		errx(1, "module %s not found", LKPI_CDEVTEST_MODULE);

	printf("Unloading %s (id %d)...\n", LKPI_CDEVTEST_MODULE, id);
	if (kldunload(id) < 0)
		err(1, "kldunload(%s)", LKPI_CDEVTEST_MODULE);

	return 0;
}

static int
run_test(void)
{
	struct lkpi_cdevtest_ioctl ioc;
	const char *msg = "lkpi-cdevtest";
	char buf[64];
	ssize_t len;
	ssize_t nread;
	int fd;

	fd = open(LKPI_CDEVTEST_DEVPATH, O_RDWR);
	if (fd < 0)
		err(1, "open(%s)", LKPI_CDEVTEST_DEVPATH);

	ioc.value = 0x1234abcdU;
	if (ioctl(fd, LKPI_CDEVTEST_IOC_SET, &ioc) < 0)
		err(1, "ioctl(LKPI_CDEVTEST_IOC_SET)");

	ioc.value = 0;
	if (ioctl(fd, LKPI_CDEVTEST_IOC_GET, &ioc) < 0)
		err(1, "ioctl(LKPI_CDEVTEST_IOC_GET)");

	if (ioc.value != 0x1234abcdU)
		errx(1, "ioctl round-trip mismatch (0x%x)", ioc.value);

	len = (ssize_t)strlen(msg);
	if (write(fd, msg, (size_t)len) != len)
		err(1, "write");

	if (lseek(fd, 0, SEEK_SET) < 0) {
		close(fd);
		fd = open(LKPI_CDEVTEST_DEVPATH, O_RDONLY);
		if (fd < 0)
			err(1, "reopen(%s)", LKPI_CDEVTEST_DEVPATH);
	}

	memset(buf, 0, sizeof(buf));
	nread = read(fd, buf, (size_t)len);
	if (nread != len)
		errx(1, "read length mismatch (%zd)", nread);
	if (memcmp(buf, msg, (size_t)len) != 0)
		errx(1, "read content mismatch");

	close(fd);
	printf("cdev test passed\n");
	return 0;
}

int
main(int argc, char **argv)
{
	if (argc > 1 && strcmp(argv[1], "pre") == 0)
		return do_pre();
	if (argc > 1 && strcmp(argv[1], "post") == 0)
		return do_post();

	return run_test();
}
