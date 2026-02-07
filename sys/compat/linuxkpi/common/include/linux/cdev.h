/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2021 Mellanox Technologies, Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef	_LINUXKPI_LINUX_CDEV_H_
#define	_LINUXKPI_LINUX_CDEV_H_

#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/kdev_t.h>
#include <linux/list.h>

#include <asm/atomic-long.h>

#ifdef __DragonFly__
#include <sys/queue.h>
#endif

struct device;
struct file_operations;
struct inode;
struct module;

#ifdef __DragonFly__
#include <sys/device.h>
extern struct dev_ops linuxdev_ops;
/* Helper functions in linux_compat.c for cdev list management */
void linux_cdev_list_add(struct linux_cdev *cdev);
void linux_cdev_list_remove(struct linux_cdev *cdev);
#else
extern struct cdevsw linuxcdevsw;
#endif
extern const struct kobj_type linux_cdev_ktype;
extern const struct kobj_type linux_cdev_static_ktype;

struct linux_cdev {
	struct kobject	kobj;
	struct module	*owner;
	struct cdev	*cdev;
	dev_t		dev;
	const struct file_operations *ops;
	u_int		refs;
	u_int		siref;
#ifdef __DragonFly__
	TAILQ_ENTRY(linux_cdev) cdev_list;
#endif
};

struct linux_cdev *cdev_alloc(void);

static inline void
cdev_init(struct linux_cdev *cdev, const struct file_operations *ops)
{

	kobject_init(&cdev->kobj, &linux_cdev_static_ktype);
	cdev->ops = ops;
	cdev->refs = 1;
}

static inline void
cdev_put(struct linux_cdev *p)
{
	kobject_put(&p->kobj);
}

static inline int
cdev_add(struct linux_cdev *cdev, dev_t dev, unsigned count)
{
#ifdef __DragonFly__
	/*
	 * DragonFly device creation works differently than FreeBSD.
	 * Use make_dev() to create the devfs node and attach linuxdev_ops.
	 */
	if (count != 1)
		return (-EINVAL);
	cdev->dev = dev;
	if (kobject_name(&cdev->kobj) == NULL)
		return (-EINVAL);
	cdev->cdev = make_dev(&linuxdev_ops, MINOR(dev), 0, 0, 0700, "%s",
	    kobject_name(&cdev->kobj));
	if (cdev->cdev == NULL)
		return (-ENOMEM);
	cdev->cdev->si_drv1 = cdev;
	if (cdev->kobj.parent != NULL)
		kobject_get(cdev->kobj.parent);
	/* Add to global list for linux_find_cdev */
	linux_cdev_list_add(cdev);
	return (0);
#else
	struct make_dev_args args;
	int error;

	if (count != 1)
		return (-EINVAL);

	cdev->dev = dev;

	/* Setup arguments for make_dev_s() */
	make_dev_args_init(&args);
	args.mda_devsw = &linuxcdevsw;
	args.mda_uid = 0;
	args.mda_gid = 0;
	args.mda_mode = 0700;
	args.mda_si_drv1 = cdev;

	error = make_dev_s(&args, &cdev->cdev, "%s",
	    kobject_name(&cdev->kobj));
	if (error)
		return (-error);

	kobject_get(cdev->kobj.parent);
	return (0);
#endif
}

static inline int
cdev_add_ext(struct linux_cdev *cdev, dev_t dev, uid_t uid, gid_t gid, int mode)
{
#ifdef __DragonFly__
	/*
	 * DragonFly device creation with extended attributes.
	 * Use make_dev() to create the devfs node and attach linuxdev_ops.
	 */
	cdev->dev = dev;
	if (kobject_name(&cdev->kobj) == NULL)
		return (-EINVAL);
	cdev->cdev = make_dev(&linuxdev_ops, MINOR(dev), uid, gid, mode, "%s/%d",
	    kobject_name(&cdev->kobj), MINOR(dev));
	if (cdev->cdev == NULL)
		return (-ENOMEM);
	cdev->cdev->si_drv1 = cdev;
	if (cdev->kobj.parent != NULL)
		kobject_get(cdev->kobj.parent);
	/* Add to global list for linux_find_cdev */
	linux_cdev_list_add(cdev);
	return (0);
#else
	struct make_dev_args args;
	int error;

	cdev->dev = dev;

	/* Setup arguments for make_dev_s() */
	make_dev_args_init(&args);
	args.mda_devsw = &linuxcdevsw;
	args.mda_uid = uid;
	args.mda_gid = gid;
	args.mda_mode = mode;
	args.mda_si_drv1 = cdev;

	error = make_dev_s(&args, &cdev->cdev, "%s/%d",
	    kobject_name(&cdev->kobj), MINOR(dev));
	if (error)
		return (-error);

	kobject_get(cdev->kobj.parent);
	return (0);
#endif
}

static inline void
cdev_del(struct linux_cdev *cdev)
{
#ifdef __DragonFly__
	/* Remove from global list */
	linux_cdev_list_remove(cdev);
#endif
	kobject_put(&cdev->kobj);
}

struct linux_cdev *linux_find_cdev(const char *name, unsigned major, unsigned minor);

int linux_cdev_device_add(struct linux_cdev *, struct device *);
void linux_cdev_device_del(struct linux_cdev *, struct device *);

#define	cdev_device_add(...)		\
  linux_cdev_device_add(__VA_ARGS__)
#define	cdev_device_del(...)		\
  linux_cdev_device_del(__VA_ARGS__)

#define	cdev	linux_cdev

#endif	/* _LINUXKPI_LINUX_CDEV_H_ */
