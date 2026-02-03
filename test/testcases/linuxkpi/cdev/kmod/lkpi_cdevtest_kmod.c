/*-
 * Copyright (c) 2026
 *	The DragonFly Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* kconfig.h is force-included by compiler flags */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/errno.h>

#include "../../../../sys/compat/linuxkpi/common/include/linux/cdev.h"
#include "../../../../sys/compat/linuxkpi/common/include/linux/fs.h"
#include "../../../../sys/compat/linuxkpi/common/include/linux/module.h"
#include "../../../../sys/compat/linuxkpi/common/include/asm/uaccess.h"

#include "../lkpi_cdevtest.h"

#define LKPI_CDEVTEST_BUFSZ 128

static struct linux_cdev *lkpi_cdev;
static char lkpi_cdev_buf[LKPI_CDEVTEST_BUFSZ];
static size_t lkpi_cdev_len;
static uint32_t lkpi_cdev_value;

static int
lkpi_cdev_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int
lkpi_cdev_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t
lkpi_cdev_read(struct file *filp, char __user *buf, size_t count,
    off_t *ppos)
{
	size_t avail;
	size_t n;

	if (*ppos < 0)
		return -EINVAL;
	if ((size_t)*ppos >= lkpi_cdev_len)
		return 0;

	avail = lkpi_cdev_len - (size_t)*ppos;
	n = (count < avail) ? count : avail;
	if (copy_to_user(buf, lkpi_cdev_buf + *ppos, n) != 0)
		return -EFAULT;

	*ppos += (off_t)n;
	return (ssize_t)n;
}

static ssize_t
lkpi_cdev_write(struct file *filp, const char __user *buf, size_t count,
    off_t *ppos)
{
	size_t n;

	if (*ppos < 0)
		return -EINVAL;

	n = (count < LKPI_CDEVTEST_BUFSZ) ? count : LKPI_CDEVTEST_BUFSZ;
	if (copy_from_user(lkpi_cdev_buf, buf, n) != 0)
		return -EFAULT;

	lkpi_cdev_len = n;
	*ppos += (off_t)n;
	return (ssize_t)n;
}

static long
lkpi_cdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct lkpi_cdevtest_ioctl ioc;

	switch (cmd) {
	case LKPI_CDEVTEST_IOC_SET:
		if (copy_from_user(&ioc, (void __user *)arg, sizeof(ioc)) != 0)
			return -EFAULT;
		lkpi_cdev_value = ioc.value;
		return 0;
	case LKPI_CDEVTEST_IOC_GET:
		ioc.value = lkpi_cdev_value;
		if (copy_to_user((void __user *)arg, &ioc, sizeof(ioc)) != 0)
			return -EFAULT;
		return 0;
	default:
		return -EINVAL;
	}
}

static const struct file_operations lkpi_cdev_fops = {
	.owner = THIS_MODULE,
	.open = lkpi_cdev_open,
	.release = lkpi_cdev_release,
	.read = lkpi_cdev_read,
	.write = lkpi_cdev_write,
	.unlocked_ioctl = lkpi_cdev_ioctl,
};

static int
lkpi_cdevtest_init(void)
{
	int error;
	dev_t dev;

	lkpi_cdev = cdev_alloc();
	if (lkpi_cdev == NULL)
		return -ENOMEM;

	lkpi_cdev->owner = THIS_MODULE;
	cdev_init(lkpi_cdev, &lkpi_cdev_fops);

	error = kobject_set_name(&lkpi_cdev->kobj, LKPI_CDEVTEST_DEVNAME);
	if (error != 0) {
		cdev_put(lkpi_cdev);
		lkpi_cdev = NULL;
		return error;
	}

	dev = MKDEV(0, 200);
	error = cdev_add(lkpi_cdev, dev, 1);
	if (error != 0) {
		cdev_put(lkpi_cdev);
		lkpi_cdev = NULL;
		return error;
	}

	lkpi_cdev_value = 0;
	lkpi_cdev_len = 0;
	return 0;
}

static void
lkpi_cdevtest_exit(void)
{
	if (lkpi_cdev != NULL) {
		cdev_del(lkpi_cdev);
		lkpi_cdev = NULL;
	}
}

module_init(lkpi_cdevtest_init);
module_exit(lkpi_cdevtest_exit);

MODULE_LICENSE("BSD");
MODULE_AUTHOR("DragonFly Project");
MODULE_DESCRIPTION("LinuxKPI cdev/devfs bridge test module");
