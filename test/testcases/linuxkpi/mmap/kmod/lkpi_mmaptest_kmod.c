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
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/spinlock.h>
#include <sys/spinlock2.h>
#include <vm/pmap.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>

#include "../../../../sys/compat/linuxkpi/common/include/linux/cdev.h"
#include "../../../../sys/compat/linuxkpi/common/include/linux/fs.h"
#include "../../../../sys/compat/linuxkpi/common/include/linux/mm.h"
#include "../../../../sys/compat/linuxkpi/common/include/linux/slab.h"
#include "../../../../sys/compat/linuxkpi/common/include/asm/uaccess.h"

#include "../lkpi_mmaptest.h"

#define LKPI_MMAPTEST_MAX_OBJS 32
#define LKPI_MMAPTEST_MAX_SIZE (4 * 1024 * 1024)

struct lkpi_mmaptest_obj {
	uint64_t handle;
	uint64_t mmap_off;
	vm_ooffset_t size;
	uint32_t cache;
	void *kva;
	vm_paddr_t paddr;
	TAILQ_ENTRY(lkpi_mmaptest_obj) entry;
};

static struct linux_cdev *lkpi_cdev;
static struct spinlock obj_lock;
static TAILQ_HEAD(, lkpi_mmaptest_obj) obj_list;
static uint64_t next_handle = 1;
static uint64_t next_off = PAGE_SIZE;
static uint32_t obj_count;

static void
lkpi_mmaptest_vma_open(struct vm_area_struct *vma)
{
	(void)vma;
}

static void
lkpi_mmaptest_vma_close(struct vm_area_struct *vma)
{
	(void)vma;
}

static const struct vm_operations_struct lkpi_mmap_vm_ops = {
	.open = lkpi_mmaptest_vma_open,
	.close = lkpi_mmaptest_vma_close,
	.fault = NULL,
	.access = NULL,
};

static struct lkpi_mmaptest_obj *
lkpi_obj_find_handle(uint64_t handle)
{
	struct lkpi_mmaptest_obj *obj;

	TAILQ_FOREACH(obj, &obj_list, entry) {
		if (obj->handle == handle)
			return obj;
	}
	return NULL;
}

static struct lkpi_mmaptest_obj *
lkpi_obj_find_off(uint64_t off)
{
	struct lkpi_mmaptest_obj *obj;

	TAILQ_FOREACH(obj, &obj_list, entry) {
		if (obj->mmap_off == off)
			return obj;
	}
	return NULL;
}

static void
lkpi_obj_free(struct lkpi_mmaptest_obj *obj)
{
	if (obj->kva != NULL)
		contigfree(obj->kva, (unsigned long)obj->size, M_TEMP);
	kfree(obj);
}

static int
lkpi_mmaptest_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int
lkpi_mmaptest_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static int
lkpi_mmaptest_mmap(struct linux_file *filp, struct vm_area_struct *vma)
{
	struct lkpi_mmaptest_obj *obj;
	uint64_t off;
	vm_size_t len;
	pgprot_t prot;

	len = (vm_size_t)(vma->vm_end - vma->vm_start);
	if (len == 0)
		return -EINVAL;

	if (!PAGE_ALIGNED(vma->vm_start) || !PAGE_ALIGNED(vma->vm_end))
		return -EINVAL;

	if ((vma->vm_flags & VM_SHARED) == 0)
		return -EINVAL;

	off = (uint64_t)vma->vm_pgoff << PAGE_SHIFT;

	spin_lock(&obj_lock);
	obj = lkpi_obj_find_off(off);
	if (obj == NULL || obj->size != (vm_ooffset_t)len) {
		spin_unlock(&obj_lock);
		return -EINVAL;
	}

	switch (obj->cache) {
	case LKPI_MMAP_CACHE_WC:
		prot = pgprot_writecombine(vm_get_page_prot(vma->vm_flags));
		break;
	case LKPI_MMAP_CACHE_UC:
		prot = pgprot_noncached(vm_get_page_prot(vma->vm_flags));
		break;
	case LKPI_MMAP_CACHE_WB:
	default:
		prot = vm_get_page_prot(vma->vm_flags);
		break;
	}
	spin_unlock(&obj_lock);

	vma->vm_page_prot = prot;
	vma->vm_pfn = (vm_paddr_t)(obj->paddr >> PAGE_SHIFT);
	vma->vm_len = len;
	vma->vm_private_data = obj;
	vma->vm_ops = &lkpi_mmap_vm_ops;

	return 0;
}

static long
lkpi_mmaptest_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct lkpi_mmaptest_obj *obj;
	struct lkpi_mmap_alloc alloc;
	struct lkpi_mmap_free fre;
	struct lkpi_mmap_query query;
	struct lkpi_mmap_memattr memattr;
	vm_ooffset_t size;
	vm_object_t vmobj;
	vm_paddr_t paddr;
	uint64_t off;
	uint64_t handle;
	uint8_t *p;
	int i;

	switch (cmd) {
	case LKPI_MMAPTEST_ALLOC:
		if (copy_from_user(&alloc, (void __user *)arg, sizeof(alloc)) != 0)
			return -EFAULT;
	if (alloc.size == 0 || !PAGE_ALIGNED(alloc.size))
		return -EINVAL;
		if (alloc.size > LKPI_MMAPTEST_MAX_SIZE)
			return -EINVAL;
		if (alloc.cache > LKPI_MMAP_CACHE_UC)
			return -EINVAL;

		size = (vm_ooffset_t)alloc.size;
		p = contigmalloc((unsigned long)size, M_TEMP, M_WAITOK, 0,
		    (vm_paddr_t)-1, PAGE_SIZE, 0);
		if (p == NULL)
			return -ENOMEM;
		for (i = 0; i < (int)size; i++)
			p[i] = (uint8_t)(0x5a + i);
		paddr = vtophys(p);

		obj = kzalloc(sizeof(*obj), GFP_KERNEL);
		if (obj == NULL) {
			contigfree(p, (unsigned long)size, M_TEMP);
			return -ENOMEM;
		}

		spin_lock(&obj_lock);
		if (obj_count >= LKPI_MMAPTEST_MAX_OBJS) {
			spin_unlock(&obj_lock);
			contigfree(p, (unsigned long)size, M_TEMP);
			kfree(obj);
			return -ENOMEM;
		}
		obj_count++;
		spin_unlock(&obj_lock);

		handle = next_handle++;
		off = next_off;
		next_off += size;

		obj->handle = handle;
		obj->mmap_off = off;
		obj->size = size;
		obj->cache = alloc.cache;
		obj->paddr = paddr;
		obj->kva = p;

		spin_lock(&obj_lock);
		TAILQ_INSERT_TAIL(&obj_list, obj, entry);
		spin_unlock(&obj_lock);

		alloc.mmap_off = off;
		alloc.handle = handle;
		if (copy_to_user((void __user *)arg, &alloc, sizeof(alloc)) != 0)
			return -EFAULT;
		return 0;
	case LKPI_MMAPTEST_FREE:
		if (copy_from_user(&fre, (void __user *)arg, sizeof(fre)) != 0)
			return -EFAULT;

		spin_lock(&obj_lock);
		obj = lkpi_obj_find_handle(fre.handle);
		if (obj != NULL)
			TAILQ_REMOVE(&obj_list, obj, entry);
		spin_unlock(&obj_lock);
		if (obj == NULL)
			return -EINVAL;

		spin_lock(&obj_lock);
		if (obj_count > 0)
			obj_count--;
		spin_unlock(&obj_lock);
		lkpi_obj_free(obj);
		return 0;
	case LKPI_MMAPTEST_QUERY:
		if (copy_from_user(&query, (void __user *)arg, sizeof(query)) != 0)
			return -EFAULT;

		spin_lock(&obj_lock);
		obj = lkpi_obj_find_handle(query.handle);
		if (obj == NULL) {
			spin_unlock(&obj_lock);
			return -EINVAL;
		}
		query.size = obj->size;
		query.cache = obj->cache;
		query.mmap_off = obj->mmap_off;
		spin_unlock(&obj_lock);

		if (copy_to_user((void __user *)arg, &query, sizeof(query)) != 0)
			return -EFAULT;
		return 0;
	case LKPI_MMAPTEST_GET_MEMATTR:
		if (copy_from_user(&memattr, (void __user *)arg, sizeof(memattr)) != 0)
			return -EFAULT;

		spin_lock(&obj_lock);
		obj = lkpi_obj_find_handle(memattr.handle);
		if (obj == NULL) {
			spin_unlock(&obj_lock);
			return -EINVAL;
		}
		spin_unlock(&obj_lock);

		vmobj = cdev_pager_lookup(obj);
		if (vmobj == NULL)
			return -EINVAL;
		if (vmobj->memattr == VM_MEMATTR_WRITE_COMBINING)
			memattr.memattr = LKPI_MMAP_CACHE_WC;
		else if (vmobj->memattr == VM_MEMATTR_UNCACHEABLE)
			memattr.memattr = LKPI_MMAP_CACHE_UC;
		else
			memattr.memattr = LKPI_MMAP_CACHE_WB;
		vm_object_deallocate(vmobj);

		if (copy_to_user((void __user *)arg, &memattr, sizeof(memattr)) != 0)
			return -EFAULT;
		return 0;
	default:
		return -EINVAL;
	}
}

static const struct file_operations lkpi_mmaptest_fops = {
	.owner = NULL,
	.open = lkpi_mmaptest_open,
	.release = lkpi_mmaptest_release,
	.mmap = lkpi_mmaptest_mmap,
	.unlocked_ioctl = lkpi_mmaptest_ioctl,
};

static int
lkpi_mmaptest_init(void)
{
	int error;
	dev_t dev;

	TAILQ_INIT(&obj_list);
	spin_init(&obj_lock, "lkpi_mmaptest");
	obj_count = 0;

	lkpi_cdev = cdev_alloc();
	if (lkpi_cdev == NULL)
		return -ENOMEM;

	lkpi_cdev->owner = NULL;
	cdev_init(lkpi_cdev, &lkpi_mmaptest_fops);

	error = kobject_set_name(&lkpi_cdev->kobj, LKPI_MMAPTEST_DEVNAME);
	if (error != 0) {
		cdev_put(lkpi_cdev);
		lkpi_cdev = NULL;
		return error;
	}

	dev = MKDEV(0, 201);
	error = cdev_add(lkpi_cdev, dev, 1);
	if (error != 0) {
		cdev_put(lkpi_cdev);
		lkpi_cdev = NULL;
		return error;
	}

	return 0;
}

static void
lkpi_mmaptest_exit(void)
{
	struct lkpi_mmaptest_obj *obj;

	spin_lock(&obj_lock);
	while ((obj = TAILQ_FIRST(&obj_list)) != NULL) {
		TAILQ_REMOVE(&obj_list, obj, entry);
		spin_unlock(&obj_lock);
		if (obj_count > 0)
			obj_count--;
		lkpi_obj_free(obj);
		spin_lock(&obj_lock);
	}
	spin_unlock(&obj_lock);

	if (lkpi_cdev != NULL) {
		cdev_del(lkpi_cdev);
		lkpi_cdev = NULL;
	}
}

static int
lkpi_mmaptest_modevent(module_t mod, int type, void *data)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		error = lkpi_mmaptest_init();
		break;
	case MOD_UNLOAD:
		lkpi_mmaptest_exit();
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return error;
}

static moduledata_t lkpi_mmaptest_mod = {
	"lkpi_mmaptest_kmod",
	lkpi_mmaptest_modevent,
	NULL
};

DECLARE_MODULE(lkpi_mmaptest_kmod, lkpi_mmaptest_mod,
    SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
MODULE_VERSION(lkpi_mmaptest_kmod, 1);
