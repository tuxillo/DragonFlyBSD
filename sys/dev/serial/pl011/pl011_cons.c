/*-
 * Copyright (c) 2026 The DragonFly Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/tty.h>
#include <sys/devfs.h>
#include <sys/ucred.h>
#include <sys/cons.h>
#include <sys/device.h>
#include <machine/vmparam.h>

#include "pl011_reg.h"

#define	PL011_QEMU_BASE	0x09000000

/*
 * Minimal device operation stubs for Phase 1 console driver.
 * These will be intercepted by the console framework.
 */
static int
pl011_open(struct dev_open_args *ap)
{
	return 0;
}

static int
pl011_close(struct dev_close_args *ap)
{
	return 0;
}

static int
pl011_read(struct dev_read_args *ap)
{
	return 0;
}

static int
pl011_write(struct dev_write_args *ap)
{
	return 0;
}

static int
pl011_ioctl(struct dev_ioctl_args *ap)
{
	return 0;
}

static struct dev_ops pl011_ops = {
	{ "pl011", 0, D_TTY },
	.d_open = pl011_open,
	.d_close = pl011_close,
	.d_read = pl011_read,
	.d_write = pl011_write,
	.d_ioctl = pl011_ioctl,
	.d_kqfilter = NULL,
	.d_revoke = NULL
};

static void
pl011_cnprobe(struct consdev *cp)
{
	cp->cn_pri = CN_REMOTE;
	cp->cn_probegood = 1;
}

static void
pl011_cninit(struct consdev *cp)
{
	/* Use DMAP mapping for UART access after MMU enablement */
	cp->cn_private = (void *)PHYS_TO_DMAP(PL011_QEMU_BASE);
}

static void
pl011_cnputc(void *arg, int c)
{
	volatile uint32_t *base = (volatile uint32_t *)arg;

	while (base[PL011_FR / 4] & PL011_FR_TXFF)
		;
	base[PL011_DR / 4] = (uint32_t)c;
}

static void
pl011_cninit_fini(struct consdev *cp)
{
	/*
	 * Create /dev/ttya for userland access.
	 * In Phase 2, this will find the device created by the bus driver.
	 */
	cp->cn_dev = make_dev(&pl011_ops, 0, 
			     UID_ROOT, GID_WHEEL, 0600, "ttya");
}

static int
pl011_cngetc(void *arg)
{
	volatile uint32_t *base = (volatile uint32_t *)arg;
	
	/* Wait for data in RX FIFO */
	while (base[PL011_FR / 4] & PL011_FR_RXFE)
		;
	
	return base[PL011_DR / 4] & 0xFF;
}

static int
pl011_cncheckc(void *arg)
{
	volatile uint32_t *base = (volatile uint32_t *)arg;

	/* Check if RX FIFO has data */
	if (base[PL011_FR / 4] & PL011_FR_RXFE)
		return -1;	/* No character available */

	return base[PL011_DR / 4] & 0xFF;
}

CONS_DRIVER(pl011, pl011_cnprobe, pl011_cninit, pl011_cninit_fini, NULL,
    pl011_cngetc, pl011_cncheckc, pl011_cnputc, NULL, NULL);
