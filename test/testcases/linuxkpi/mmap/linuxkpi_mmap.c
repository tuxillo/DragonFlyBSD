#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
int kldload(const char *);
int kldunload(int);
int kldfind(const char *);
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lkpi_mmaptest.h"

#define LKPI_MMAPTEST_DEVPATH "/dev/" LKPI_MMAPTEST_DEVNAME

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
		LKPI_MMAPTEST_KO_PATH,
		"kmod/obj/lkpi_mmaptest_kmod.ko",
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
		LKPI_MMAPTEST_KO_PATH,
		LKPI_MMAPTEST_MODULE ".ko",
		LKPI_MMAPTEST_MODULE,
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

	if (wait_for_device(LKPI_MMAPTEST_DEVPATH, 200, 10000) != 0)
		errx(1, "device node %s not found", LKPI_MMAPTEST_DEVPATH);

	return 0;
}

static int
do_post(void)
{
	int id;

	id = find_module_id();
	if (id < 0)
		errx(1, "module %s not found", LKPI_MMAPTEST_MODULE);

	printf("Unloading %s (id %d)...\n", LKPI_MMAPTEST_MODULE, id);
	if (kldunload(id) < 0)
		err(1, "kldunload(%s)", LKPI_MMAPTEST_MODULE);

	return 0;
}

static void
expect_errno(int expected, const char *label)
{
	if (errno != expected)
		err(1, "%s: expected errno %d", label, expected);
}

static void
verify_pattern(const uint8_t *buf, size_t len, uint8_t seed)
{
	size_t i;

	for (i = 0; i < len; i++) {
		if (buf[i] != (uint8_t)(seed + i))
			errx(1, "pattern mismatch at %zu", i);
	}
}

static void
fill_pattern(uint8_t *buf, size_t len, uint8_t seed)
{
	size_t i;

	for (i = 0; i < len; i++)
		buf[i] = (uint8_t)(seed + i);
}

static int
run_test(void)
{
	struct lkpi_mmap_alloc alloc;
	struct lkpi_mmap_free fre;
	struct lkpi_mmap_memattr memattr;
	struct lkpi_mmap_query query;
	uint8_t *map;
	size_t size;
	int fd;
	int ps;

	ps = getpagesize();
	if (ps <= 0)
		err(1, "getpagesize");
	size = (size_t)ps * 4;

	fd = open(LKPI_MMAPTEST_DEVPATH, O_RDWR);
	if (fd < 0)
		err(1, "open(%s)", LKPI_MMAPTEST_DEVPATH);

	memset(&alloc, 0, sizeof(alloc));
	alloc.size = size;
	alloc.cache = LKPI_MMAP_CACHE_WC;
	if (ioctl(fd, LKPI_MMAPTEST_ALLOC, &alloc) < 0)
		err(1, "ioctl(LKPI_MMAPTEST_ALLOC)");
	if (alloc.mmap_off == 0 || alloc.handle == 0)
		errx(1, "invalid alloc response");

	map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
	    (off_t)alloc.mmap_off);
	if (map == MAP_FAILED)
		err(1, "mmap(MAP_SHARED)");

	verify_pattern(map, size, 0x5a);
	fill_pattern(map, size, 0x80);
	verify_pattern(map, size, 0x80);

	memset(&query, 0, sizeof(query));
	query.handle = alloc.handle;
	if (ioctl(fd, LKPI_MMAPTEST_QUERY, &query) < 0)
		err(1, "ioctl(LKPI_MMAPTEST_QUERY)");
	if (query.size != size)
		errx(1, "query size mismatch");
	if (query.mmap_off != alloc.mmap_off)
		errx(1, "query offset mismatch");
	if (query.cache != alloc.cache)
		errx(1, "query cache mismatch");

	memset(&memattr, 0, sizeof(memattr));
	memattr.handle = alloc.handle;
	if (ioctl(fd, LKPI_MMAPTEST_GET_MEMATTR, &memattr) < 0)
		err(1, "ioctl(LKPI_MMAPTEST_GET_MEMATTR)");
	if (alloc.cache == LKPI_MMAP_CACHE_WC) {
		if (memattr.memattr != LKPI_MMAP_CACHE_WC &&
		    memattr.memattr != LKPI_MMAP_CACHE_UC)
			errx(1, "memattr mismatch for WC (%u)", memattr.memattr);
	} else if (memattr.memattr != alloc.cache) {
		errx(1, "memattr mismatch (%u)", memattr.memattr);
	}

	if (munmap(map, size) < 0)
		err(1, "munmap");

	map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd,
	    (off_t)alloc.mmap_off);
	if (map != MAP_FAILED)
		errx(1, "MAP_PRIVATE should fail");
	expect_errno(EINVAL, "MAP_PRIVATE errno");

	map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
	    (off_t)alloc.mmap_off + 1);
	if (map != MAP_FAILED)
		errx(1, "unaligned offset should fail");
	expect_errno(EINVAL, "unaligned offset errno");

	map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
	    (off_t)(alloc.mmap_off + (uint64_t)size + (uint64_t)ps));
	if (map != MAP_FAILED)
		errx(1, "invalid offset should fail");
	expect_errno(EINVAL, "invalid offset errno");

	fre.handle = alloc.handle;
	if (ioctl(fd, LKPI_MMAPTEST_FREE, &fre) < 0)
		err(1, "ioctl(LKPI_MMAPTEST_FREE)");

	close(fd);
	printf("mmap test passed\n");
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
