/* Public domain. */
/*
 * Stub module to satisfy drm-kmod's MODULE_DEPEND(drmn, mem, 1, 1, 1).
 * On FreeBSD, this is provided by sys/dev/mem/memdev.c.
 * On DragonFly, /dev/mem is built into the kernel.
 */

#include <sys/param.h>
#include <sys/module.h>

MODULE_VERSION(mem, 1);
