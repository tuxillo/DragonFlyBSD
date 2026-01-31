/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013, 2014 Mellanox Technologies, Ltd.
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
#ifndef	_LINUXKPI_LINUX_KDEV_T_H_
#define	_LINUXKPI_LINUX_KDEV_T_H_

#include <sys/types.h>

#ifdef __DragonFly__
/*
 * DragonFly's minor()/major() expect a cdev_t (struct cdev pointer),
 * not a dev_t (integer). LinuxKPI uses dev_t integers.
 *
 * On DragonFly, dev_t is defined as __uint32_t (sys/types.h).
 * Major is bits 8-15, minor is bits 0-7 and 16-31 (see sys/_devt.h).
 *
 * We provide our own MAJOR/MINOR macros for integer dev_t.
 */
#define	MAJOR(dev)	(((dev) >> 8) & 0xff)
#define	MINOR(dev)	(((dev) & 0xff) | (((dev) >> 8) & 0xffff00))
#define	MKDEV(ma, mi)	((((ma) & 0xff) << 8) | ((mi) & 0xff) | (((mi) & 0xffff00) << 8))
#else
#define MAJOR(dev)      major(dev)
#define MINOR(dev)      minor(dev)
#define MKDEV(ma, mi)   makedev(ma, mi)
#endif

static inline uint16_t
old_encode_dev(dev_t dev)
{
	return ((MAJOR(dev) << 8) | MINOR(dev));
}

#endif	/* _LINUXKPI_LINUX_KDEV_T_H_ */
