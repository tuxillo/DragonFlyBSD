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

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_param.h>

#include <sys/tree.h>

#include "../../../../sys/compat/linuxkpi/common/include/linux/cdev.h"
#include "../../../../sys/compat/linuxkpi/common/include/linux/fs.h"
#include "../../../../sys/compat/linuxkpi/common/include/linux/slab.h"
#include "../../../../sys/compat/linuxkpi/common/include/asm/uaccess.h"
#include "../../../../sys/compat/linuxkpi/common/include/vm/vm_radix.h"

#include "../lkpi_vm_radix.h"

static struct linux_cdev *lkpi_cdev;

static void
lkpi_vmradix_fail(struct lkpi_vmradix_result *res, uint32_t subtest, int err)
{
	res->status = 1;
	res->subtest = subtest;
	res->err = err;
}

static vm_object_t
lkpi_vmradix_make_object(const vm_pindex_t *indices, size_t count)
{
	vm_object_t obj;
	vm_page_t m;
	vm_pindex_t pindex;
	size_t i;

	obj = vm_object_allocate(OBJT_DEFAULT, count + 16);
	if (obj == NULL)
		return NULL;

	vm_object_hold(obj);
	for (i = 0; i < count; i++) {
		pindex = indices[i];
		m = vm_page_grab(obj, pindex, VM_ALLOC_NORMAL | VM_ALLOC_RETRY);
		if (m == NULL) {
			vm_object_drop(obj);
			vm_object_deallocate(obj);
			return NULL;
		}
		vm_page_wakeup(m);
	}
	vm_object_drop(obj);

	return obj;
}

static void
lkpi_vmradix_destroy_object(vm_object_t obj)
{
	if (obj == NULL)
		return;
	vm_object_deallocate(obj);
}

static int
lkpi_vmradix_check_sequence(vm_object_t obj, const vm_pindex_t *expected,
    size_t expected_count)
{
	struct pctrie_iter it;
	vm_page_t m;
	size_t i = 0;

	vm_page_iter_init(&it, obj);
	VM_RADIX_FOREACH(m, &it) {
		if (i >= expected_count)
			return EINVAL;
		if (m->pindex != expected[i])
			return EINVAL;
		i++;
	}
	if (i != expected_count)
		return EINVAL;
	return 0;
}

static int
lkpi_vmradix_foreach(vm_object_t obj)
{
	struct pctrie_iter it;
	vm_page_t m;
	const vm_pindex_t expected[] = {0, 2, 10};
	size_t i;
	int error;

	vm_object_hold(obj);

	error = lkpi_vmradix_check_sequence(obj, expected,
	    sizeof(expected) / sizeof(expected[0]));
	if (error != 0)
		goto out;

	vm_page_iter_init(&it, obj);
	i = 0;
	VM_RADIX_FOREACH_FROM(m, &it, 1) {
		if (i == 0 && m->pindex != 2) {
			error = EINVAL;
			goto out;
		}
		if (i == 1 && m->pindex != 10) {
			error = EINVAL;
			goto out;
		}
		i++;
	}
	if (i != 2) {
		error = EINVAL;
		goto out;
	}

	vm_page_iter_init(&it, obj);
	m = vm_radix_iter_lookup_ge(&it, 3);
	if (m == NULL || m->pindex != 10) {
		error = EINVAL;
		goto out;
	}

	vm_page_iter_init(&it, obj);
	m = vm_radix_iter_lookup_ge(&it, 11);
	if (m != NULL) {
		error = EINVAL;
		goto out;
	}

out:
	vm_object_drop(obj);
	return error;
}

static int
lkpi_vmradix_forall_holes(vm_object_t obj)
{
	struct pctrie_iter it;
	vm_page_t m;
	int count;
	int error = 0;

	vm_object_hold(obj);
	vm_page_iter_init(&it, obj);
	count = 0;
	VM_RADIX_FORALL_FROM(m, &it, 0) {
		if (count == 0 && m->pindex != 0)
			error = EINVAL;
		count++;
	}
	if (error == 0 && count != 1)
		error = EINVAL;

	vm_page_iter_init(&it, obj);
	count = 0;
	VM_RADIX_FORALL_FROM(m, &it, 2) {
		if (count == 0 && m->pindex != 2)
			error = EINVAL;
		count++;
	}
	if (error == 0 && count != 1)
		error = EINVAL;

	vm_object_drop(obj);
	return error;
}

static int
lkpi_vmradix_forall_dense(vm_object_t obj)
{
	struct pctrie_iter it;
	vm_page_t m;
	int count;
	int error = 0;

	vm_object_hold(obj);
	vm_page_iter_init(&it, obj);
	count = 0;
	VM_RADIX_FORALL_FROM(m, &it, 10) {
		if ((count == 0 && m->pindex != 10) ||
		    (count == 1 && m->pindex != 11) ||
		    (count == 2 && m->pindex != 12))
			error = EINVAL;
		count++;
	}
	if (error == 0 && count != 3)
		error = EINVAL;

	vm_page_iter_init(&it, obj);
	count = 0;
	VM_RADIX_FORALL_FROM(m, &it, 11) {
		if ((count == 0 && m->pindex != 11) ||
		    (count == 1 && m->pindex != 12))
			error = EINVAL;
		count++;
	}
	if (error == 0 && count != 2)
		error = EINVAL;

	vm_page_iter_init(&it, obj);
	m = vm_radix_iter_lookup(&it, 9);
	if (m != NULL)
		error = EINVAL;

	vm_object_drop(obj);
	return error;
}

static int
lkpi_vmradix_lookup(vm_object_t obj)
{
	struct pctrie_iter it;
	vm_page_t m;
	int error = 0;

	vm_object_hold(obj);
	vm_page_iter_init(&it, obj);
	m = vm_radix_iter_lookup_ge(&it, 9);
	if (m == NULL || m->pindex != 10)
		error = EINVAL;

	vm_page_iter_init(&it, obj);
	m = vm_radix_iter_lookup_le(&it, 9);
	if (m == NULL || m->pindex != 2)
		error = EINVAL;

	vm_page_iter_init(&it, obj);
	m = vm_radix_iter_lookup_lt(&it, 1);
	if (m == NULL || m->pindex != 0)
		error = EINVAL;

	vm_page_iter_init(&it, obj);
	m = vm_radix_iter_lookup_lt(&it, 0);
	if (m != NULL)
		error = EINVAL;

	vm_object_drop(obj);
	return error;
}

static int
lkpi_vmradix_step_prev(vm_object_t obj)
{
	struct pctrie_iter it;
	vm_page_t m;
	int error = 0;

	vm_object_hold(obj);
	vm_page_iter_init(&it, obj);
	m = vm_radix_iter_lookup_ge(&it, 0);
	if (m == NULL || m->pindex != 0)
		error = EINVAL;
	m = vm_radix_iter_step(&it);
	if (m == NULL || m->pindex != 2)
		error = EINVAL;
	m = vm_radix_iter_step(&it);
	if (m == NULL || m->pindex != 10)
		error = EINVAL;
	m = vm_radix_iter_step(&it);
	if (m != NULL)
		error = EINVAL;

	vm_page_iter_init(&it, obj);
	m = vm_radix_iter_lookup_le(&it, 10);
	if (m == NULL || m->pindex != 10)
		error = EINVAL;
	m = vm_radix_iter_prev(&it);
	if (m == NULL || m->pindex != 2)
		error = EINVAL;
	m = vm_radix_iter_prev(&it);
	if (m == NULL || m->pindex != 0)
		error = EINVAL;
	m = vm_radix_iter_prev(&it);
	if (m != NULL)
		error = EINVAL;

	vm_object_drop(obj);
	return error;
}

static int
lkpi_vmradix_limit(vm_object_t obj)
{
	struct pctrie_iter it;
	vm_page_t m;
	int count = 0;
	int error = 0;

	vm_object_hold(obj);
	vm_page_iter_limit_init(&it, obj, 2);
	VM_RADIX_FOREACH(m, &it) {
		if (count == 0 && m->pindex != 0)
			error = EINVAL;
		if (count == 1 && m->pindex != 2)
			error = EINVAL;
		count++;
	}
	if (error == 0 && count != 2)
		error = EINVAL;

	vm_page_iter_limit_init(&it, obj, 2);
	m = vm_radix_iter_lookup_ge(&it, 3);
	if (m != NULL)
		error = EINVAL;

	vm_object_drop(obj);
	return error;
}

static int
lkpi_vmradix_run(struct lkpi_vmradix_result *res)
{
	const vm_pindex_t sparse_idx[] = {0, 2, 10};
	const vm_pindex_t dense_idx[] = {10, 11, 12};
	vm_object_t sparse;
	vm_object_t dense;
	int error;

	res->status = 0;
	res->subtest = 0;
	res->err = 0;

	sparse = lkpi_vmradix_make_object(sparse_idx,
	    sizeof(sparse_idx) / sizeof(sparse_idx[0]));
	if (sparse == NULL) {
		lkpi_vmradix_fail(res, LKPI_VMRADIX_SUBTEST_FOREACH, ENOMEM);
		return 0;
	}
	dense = lkpi_vmradix_make_object(dense_idx,
	    sizeof(dense_idx) / sizeof(dense_idx[0]));
	if (dense == NULL) {
		lkpi_vmradix_fail(res, LKPI_VMRADIX_SUBTEST_FORALL_DENSE, ENOMEM);
		lkpi_vmradix_destroy_object(sparse);
		return 0;
	}

	error = lkpi_vmradix_foreach(sparse);
	if (error != 0) {
		lkpi_vmradix_fail(res, LKPI_VMRADIX_SUBTEST_FOREACH, error);
		goto out;
	}

	error = lkpi_vmradix_forall_holes(sparse);
	if (error != 0) {
		lkpi_vmradix_fail(res, LKPI_VMRADIX_SUBTEST_FORALL_HOLES, error);
		goto out;
	}

	error = lkpi_vmradix_forall_dense(dense);
	if (error != 0) {
		lkpi_vmradix_fail(res, LKPI_VMRADIX_SUBTEST_FORALL_DENSE, error);
		goto out;
	}

	error = lkpi_vmradix_lookup(sparse);
	if (error != 0) {
		lkpi_vmradix_fail(res, LKPI_VMRADIX_SUBTEST_LOOKUP, error);
		goto out;
	}

	error = lkpi_vmradix_step_prev(sparse);
	if (error != 0) {
		lkpi_vmradix_fail(res, LKPI_VMRADIX_SUBTEST_STEP_PREV, error);
		goto out;
	}

	error = lkpi_vmradix_limit(sparse);
	if (error != 0) {
		lkpi_vmradix_fail(res, LKPI_VMRADIX_SUBTEST_LIMIT, error);
		goto out;
	}

out:
	lkpi_vmradix_destroy_object(sparse);
	lkpi_vmradix_destroy_object(dense);
	return 0;
}

static int
lkpi_vmradix_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int
lkpi_vmradix_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static long
lkpi_vmradix_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct lkpi_vmradix_result res;

	if (cmd != LKPI_VMRADIX_RUN)
		return -EINVAL;

	bzero(&res, sizeof(res));
	lkpi_vmradix_run(&res);

	if (copy_to_user((void __user *)arg, &res, sizeof(res)) != 0)
		return -EFAULT;
	return 0;
}

static const struct file_operations lkpi_vmradix_fops = {
	.owner = NULL,
	.open = lkpi_vmradix_open,
	.release = lkpi_vmradix_release,
	.unlocked_ioctl = lkpi_vmradix_ioctl,
};

static int
lkpi_vmradix_init(void)
{
	int error;
	dev_t dev;

	lkpi_cdev = cdev_alloc();
	if (lkpi_cdev == NULL)
		return -ENOMEM;

	lkpi_cdev->owner = NULL;
	cdev_init(lkpi_cdev, &lkpi_vmradix_fops);

	error = kobject_set_name(&lkpi_cdev->kobj, LKPI_VMRADIX_DEVNAME);
	if (error != 0) {
		cdev_put(lkpi_cdev);
		lkpi_cdev = NULL;
		return error;
	}

	dev = MKDEV(0, 204);
	error = cdev_add(lkpi_cdev, dev, 1);
	if (error != 0) {
		cdev_put(lkpi_cdev);
		lkpi_cdev = NULL;
		return error;
	}

	return 0;
}

static void
lkpi_vmradix_exit(void)
{
	if (lkpi_cdev != NULL) {
		cdev_del(lkpi_cdev);
		lkpi_cdev = NULL;
	}
}

static int
lkpi_vmradix_modevent(module_t mod, int type, void *data)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		error = lkpi_vmradix_init();
		break;
	case MOD_UNLOAD:
		lkpi_vmradix_exit();
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return error;
}

static moduledata_t lkpi_vmradix_mod = {
	"lkpi_vm_radixtest_kmod",
	lkpi_vmradix_modevent,
	NULL
};

DECLARE_MODULE(lkpi_vm_radixtest_kmod, lkpi_vmradix_mod,
    SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
MODULE_VERSION(lkpi_vm_radixtest_kmod, 1);
