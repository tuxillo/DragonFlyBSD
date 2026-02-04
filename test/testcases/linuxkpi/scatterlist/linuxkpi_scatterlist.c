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

#include "lkpi_sgtest.h"

#define LKPI_SGTEST_DEVPATH "/dev/" LKPI_SGTEST_DEVNAME

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
		LKPI_SGTEST_KO_PATH,
		"kmod/obj/lkpi_sgtest_kmod.ko",
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
		LKPI_SGTEST_KO_PATH,
		LKPI_SGTEST_MODULE ".ko",
		LKPI_SGTEST_MODULE,
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

	if (wait_for_device(LKPI_SGTEST_DEVPATH, 200, 10000) != 0)
		errx(1, "device node %s not found", LKPI_SGTEST_DEVPATH);

	return 0;
}

static int
do_post(void)
{
	int id;

	id = find_module_id();
	if (id < 0)
		errx(1, "module %s not found", LKPI_SGTEST_MODULE);

	printf("Unloading %s (id %d)...\n", LKPI_SGTEST_MODULE, id);
	if (kldunload(id) < 0)
		err(1, "kldunload(%s)", LKPI_SGTEST_MODULE);

	return 0;
}

static int
run_test(void)
{
	struct lkpi_sgtest_result res;
	int fd;

	fd = open(LKPI_SGTEST_DEVPATH, O_RDWR);
	if (fd < 0)
		err(1, "open(%s)", LKPI_SGTEST_DEVPATH);

	memset(&res, 0, sizeof(res));
	if (ioctl(fd, LKPI_SGTEST_RUN, &res) < 0)
		err(1, "ioctl(LKPI_SGTEST_RUN)");

	if (res.status != 0)
		errx(1, "subtest %u failed (err=%d)", res.subtest, res.err);

	close(fd);
	printf("scatterlist test passed\n");
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
