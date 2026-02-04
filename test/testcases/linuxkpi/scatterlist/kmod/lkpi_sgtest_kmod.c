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
#include <sys/module.h>

#include "../../../../sys/compat/linuxkpi/common/include/linux/cdev.h"
#include "../../../../sys/compat/linuxkpi/common/include/linux/fs.h"
#include "../../../../sys/compat/linuxkpi/common/include/linux/gfp.h"
#include "../../../../sys/compat/linuxkpi/common/include/linux/highmem.h"
#include "../../../../sys/compat/linuxkpi/common/include/linux/scatterlist.h"
#include "../../../../sys/compat/linuxkpi/common/include/linux/slab.h"
#include "../../../../sys/compat/linuxkpi/common/include/asm/uaccess.h"

#include "../lkpi_sgtest.h"

#define LKPI_SGTEST_MAX_PAGES 4

static struct linux_cdev *lkpi_cdev;

static void
lkpi_sgtest_fail(struct lkpi_sgtest_result *res, uint32_t subtest, int err)
{
	res->status = 1;
	res->subtest = subtest;
	res->err = err;
}

static void
lkpi_fill_pattern(uint8_t *buf, size_t len, uint8_t seed)
{
	size_t i;

	for (i = 0; i < len; i++)
		buf[i] = (uint8_t)(seed + i);
}

static int
lkpi_verify_pattern(const uint8_t *buf, size_t len, uint8_t seed)
{
	size_t i;

	for (i = 0; i < len; i++) {
		if (buf[i] != (uint8_t)(seed + i))
			return (EINVAL);
	}
	return (0);
}

static int
lkpi_sgtest_highmem(void)
{
	struct page *pages[LKPI_SGTEST_MAX_PAGES];
	uint8_t *tmp;
	uint8_t *addr;
	int error;
	int i;

	for (i = 0; i < LKPI_SGTEST_MAX_PAGES; i++)
		pages[i] = NULL;

	tmp = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (tmp == NULL)
		return (ENOMEM);

	for (i = 0; i < LKPI_SGTEST_MAX_PAGES; i++) {
		pages[i] = alloc_page(GFP_KERNEL);
		if (pages[i] == NULL) {
			error = ENOMEM;
			goto out;
		}
		lkpi_fill_pattern(tmp, PAGE_SIZE, (uint8_t)(0x10 + i));
		memcpy_to_page(pages[i], 0, (const char *)tmp, PAGE_SIZE);

		bzero(tmp, PAGE_SIZE);
		memcpy_from_page((char *)tmp, pages[i], 0, PAGE_SIZE);
		error = lkpi_verify_pattern(tmp, PAGE_SIZE, (uint8_t)(0x10 + i));
		if (error != 0)
			goto out;

		addr = kmap_local_page(pages[i]);
		if (addr == NULL) {
			error = ENOMEM;
			goto out;
		}
		lkpi_fill_pattern(addr, PAGE_SIZE, (uint8_t)(0x40 + i));
		kunmap_local(addr);

		bzero(tmp, PAGE_SIZE);
		memcpy_from_page((char *)tmp, pages[i], 0, PAGE_SIZE);
		error = lkpi_verify_pattern(tmp, PAGE_SIZE, (uint8_t)(0x40 + i));
		if (error != 0)
			goto out;
	}

	error = 0;
out:
	for (i = 0; i < LKPI_SGTEST_MAX_PAGES; i++) {
		if (pages[i] != NULL)
			__free_page(pages[i]);
	}
	if (tmp != NULL)
		kfree(tmp);
	return (error);
}

static int
lkpi_sgtest_sg_table(void)
{
	struct sg_table sgt;
	struct scatterlist *sg;
	struct page *pages[3];
	uint64_t total;
	uint64_t expected_total;
	unsigned int nents;
	unsigned int i;
	int error;

	for (i = 0; i < 3; i++)
		pages[i] = alloc_page(GFP_KERNEL);
	for (i = 0; i < 3; i++) {
		if (pages[i] == NULL) {
			error = ENOMEM;
			goto out;
		}
	}

	bzero(&sgt, sizeof(sgt));
	error = sg_alloc_table_from_pages(&sgt, pages, 3, 128,
	    PAGE_SIZE * 2 + 256, PAGE_SIZE);
	if (error != 0)
		goto out;

	if (sgt.orig_nents != 3) {
		error = EINVAL;
		goto out_free;
	}

	nents = sg_nents(sgt.sgl);
	if (nents != 3) {
		error = EINVAL;
		goto out_free;
	}

	expected_total = (PAGE_SIZE - 128) + PAGE_SIZE + 256;
	total = 0;
	for_each_sg(sgt.sgl, sg, sgt.orig_nents, i) {
		switch (i) {
		case 0:
			if (sg_page(sg) != pages[0] || sg->offset != 128 ||
			    sg->length != (PAGE_SIZE - 128)) {
				error = EINVAL;
				goto out_free;
			}
			break;
		case 1:
			if (sg_page(sg) != pages[1] || sg->offset != 0 ||
			    sg->length != PAGE_SIZE) {
				error = EINVAL;
				goto out_free;
			}
			break;
		case 2:
			if (sg_page(sg) != pages[2] || sg->offset != 0 ||
			    sg->length != 256 || !sg_is_last(sg)) {
				error = EINVAL;
				goto out_free;
			}
			break;
		default:
			error = EINVAL;
			goto out_free;
		}
		total += sg->length;
	}

	if (total != expected_total) {
		error = EINVAL;
		goto out_free;
	}

	error = 0;
out_free:
	sg_free_table(&sgt);
out:
	for (i = 0; i < 3; i++) {
		if (pages[i] != NULL)
			__free_page(pages[i]);
	}
	return (error);
}

static int
lkpi_sgtest_sg_copy(void)
{
	struct sg_table sgt;
	struct page *pages[2];
	uint8_t *inbuf;
	uint8_t *outbuf;
	uint8_t *outpart;
	unsigned int size;
	int error;
	int i;
	unsigned int copied;

	for (i = 0; i < 2; i++)
		pages[i] = alloc_page(GFP_KERNEL);
	for (i = 0; i < 2; i++) {
		if (pages[i] == NULL) {
			error = ENOMEM;
			goto out;
		}
	}

	bzero(&sgt, sizeof(sgt));
	size = PAGE_SIZE * 2;
	error = sg_alloc_table_from_pages(&sgt, pages, 2, 0, size, PAGE_SIZE);
	if (error != 0)
		goto out;

	inbuf = kmalloc(size, GFP_KERNEL);
	outbuf = kmalloc(size, GFP_KERNEL);
	outpart = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (inbuf == NULL || outbuf == NULL || outpart == NULL) {
		error = ENOMEM;
		goto out_free;
	}

	lkpi_fill_pattern(inbuf, size, 0x90);
	bzero(outbuf, size);
	bzero(outpart, PAGE_SIZE);

	copied = sg_copy_from_buffer(sgt.sgl, sgt.orig_nents, inbuf, size);
	if (copied != size) {
		error = EINVAL;
		goto out_free;
	}

	copied = sg_copy_to_buffer(sgt.sgl, sgt.orig_nents, outbuf, size);
	if (copied != size) {
		error = EINVAL;
		goto out_free;
	}
	if (memcmp(inbuf, outbuf, size) != 0) {
		error = EINVAL;
		goto out_free;
	}

	copied = sg_pcopy_to_buffer(sgt.sgl, sgt.orig_nents, outpart, PAGE_SIZE,
	    128);
	if (copied != PAGE_SIZE) {
		error = EINVAL;
		goto out_free;
	}
	if (memcmp(outpart, inbuf + 128, PAGE_SIZE) != 0) {
		error = EINVAL;
		goto out_free;
	}

	error = 0;
out_free:
	if (inbuf != NULL)
		kfree(inbuf);
	if (outbuf != NULL)
		kfree(outbuf);
	if (outpart != NULL)
		kfree(outpart);
	sg_free_table(&sgt);
out:
	for (i = 0; i < 2; i++) {
		if (pages[i] != NULL)
			__free_page(pages[i]);
	}
	return (error);
}

static int
lkpi_sgtest_sg_chain(void)
{
	struct sg_table sgt;
	struct scatterlist *sg;
	struct page *page;
	unsigned int nents;
	unsigned int count;
	unsigned int i;
	int error;

	nents = SG_MAX_SINGLE_ALLOC + 4;
	page = alloc_page(GFP_KERNEL);
	if (page == NULL)
		return (ENOMEM);

	bzero(&sgt, sizeof(sgt));
	error = sg_alloc_table(&sgt, nents, GFP_KERNEL);
	if (error != 0)
		goto out;

	for_each_sg(sgt.sgl, sg, sgt.orig_nents, i)
		sg_set_page(sg, page, 16, 0);

	count = 0;
	for_each_sg(sgt.sgl, sg, sgt.orig_nents, i)
		count++;
	if (count != nents) {
		error = EINVAL;
		goto out_free;
	}

	if (!sg_is_last(&sgt.sgl[nents - 1])) {
		error = EINVAL;
		goto out_free;
	}

	error = 0;
out_free:
	sg_free_table(&sgt);
out:
	__free_page(page);
	return (error);
}

static int
lkpi_sgtest_run(struct lkpi_sgtest_result *res)
{
	int error;

	res->status = 0;
	res->subtest = 0;
	res->err = 0;

	error = lkpi_sgtest_highmem();
	if (error != 0) {
		lkpi_sgtest_fail(res, LKPI_SGTEST_SUBTEST_HIGHMEM, error);
		return 0;
	}

	error = lkpi_sgtest_sg_table();
	if (error != 0) {
		lkpi_sgtest_fail(res, LKPI_SGTEST_SUBTEST_SG_TABLE, error);
		return 0;
	}

	error = lkpi_sgtest_sg_copy();
	if (error != 0) {
		lkpi_sgtest_fail(res, LKPI_SGTEST_SUBTEST_SG_COPY, error);
		return 0;
	}

	error = lkpi_sgtest_sg_chain();
	if (error != 0) {
		lkpi_sgtest_fail(res, LKPI_SGTEST_SUBTEST_SG_CHAIN, error);
		return 0;
	}

	return 0;
}

static int
lkpi_sgtest_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int
lkpi_sgtest_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static long
lkpi_sgtest_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct lkpi_sgtest_result res;

	if (cmd != LKPI_SGTEST_RUN)
		return -EINVAL;

	bzero(&res, sizeof(res));
	lkpi_sgtest_run(&res);

	if (copy_to_user((void __user *)arg, &res, sizeof(res)) != 0)
		return -EFAULT;
	return 0;
}

static const struct file_operations lkpi_sgtest_fops = {
	.owner = NULL,
	.open = lkpi_sgtest_open,
	.release = lkpi_sgtest_release,
	.unlocked_ioctl = lkpi_sgtest_ioctl,
};

static int
lkpi_sgtest_init(void)
{
	int error;
	dev_t dev;

	lkpi_cdev = cdev_alloc();
	if (lkpi_cdev == NULL)
		return -ENOMEM;

	lkpi_cdev->owner = NULL;
	cdev_init(lkpi_cdev, &lkpi_sgtest_fops);

	error = kobject_set_name(&lkpi_cdev->kobj, LKPI_SGTEST_DEVNAME);
	if (error != 0) {
		cdev_put(lkpi_cdev);
		lkpi_cdev = NULL;
		return error;
	}

	dev = MKDEV(0, 203);
	error = cdev_add(lkpi_cdev, dev, 1);
	if (error != 0) {
		cdev_put(lkpi_cdev);
		lkpi_cdev = NULL;
		return error;
	}

	return 0;
}

static void
lkpi_sgtest_exit(void)
{
	if (lkpi_cdev != NULL) {
		cdev_del(lkpi_cdev);
		lkpi_cdev = NULL;
	}
}

static int
lkpi_sgtest_modevent(module_t mod, int type, void *data)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		error = lkpi_sgtest_init();
		break;
	case MOD_UNLOAD:
		lkpi_sgtest_exit();
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return error;
}

static moduledata_t lkpi_sgtest_mod = {
	"lkpi_sgtest_kmod",
	lkpi_sgtest_modevent,
	NULL
};

DECLARE_MODULE(lkpi_sgtest_kmod, lkpi_sgtest_mod,
    SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
MODULE_VERSION(lkpi_sgtest_kmod, 1);
