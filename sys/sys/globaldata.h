/*-
 * Copyright (c) Peter Wemm <peter@netplex.com.au> All rights reserved.
 * Copyright (c) 2003 Matthew Dillon <dillon@backplane.net> All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/i386/include/globaldata.h,v 1.11.2.1 2000/05/16 06:58:10 dillon Exp $
 * $DragonFly: src/sys/sys/globaldata.h,v 1.19 2003/11/20 06:05:31 dillon Exp $
 */

#ifndef _SYS_GLOBALDATA_H_
#define _SYS_GLOBALDATA_H_

#if defined(_KERNEL) || defined(_KERNEL_STRUCTURES)

#ifndef _SYS_TIME_H_
#include <sys/time.h>	/* struct timeval */
#endif
#ifndef _SYS_VMMETER_H_
#include <sys/vmmeter.h> /* struct vmmeter */
#endif
#ifndef _SYS_THREAD_H_
#include <sys/thread.h>	/* struct thread */
#endif
#ifndef _SYS_SLABALLOC_H_
#include <sys/slaballoc.h> /* SLGlobalData */
#endif

/*
 * This structure maps out the global data that needs to be kept on a
 * per-cpu basis.  genassym uses this to generate offsets for the assembler
 * code.  The machine-dependant portions of this file can be found in
 * <machine/globaldata.h>, but only MD code should retrieve it.
 *
 * The SMP parts are setup in pmap.c and locore.s for the BSP, and
 * mp_machdep.c sets up the data for the AP's to "see" when they awake.
 * The reason for doing it via a struct is so that an array of pointers
 * to each CPU's data can be set up for things like "check curproc on all
 * other processors"
 *
 * NOTE! this structure needs to remain compatible between module accessors
 * and the kernel, so we can't throw in lots of #ifdef's.
 *
 * gd_reqflags serves serveral purposes, but it is primarily an interrupt
 * rollup flag used by the task switcher and spl mechanisms to decide that
 * further checks are necessary.  Interrupts are typically managed on a
 * per-processor basis at least until you leave a critical section, but
 * may then be scheduled to other cpus.
 *
 * gd_vme_avail and gd_vme_base cache free vm_map_entry structures for use
 * in various vm_map related operations.  gd_vme_avail is *NOT* a count of
 * the number of structures in the cache but is instead a count of the number
 * of unreserved structures in the cache.  See vm_map_entry_reserve().
 */

struct sysmsg;
struct privatespace;
struct vm_map_entry;

struct globaldata {
	struct privatespace *gd_prvspace;	/* self-reference */
	struct thread	*gd_curthread;
	int		gd_tdfreecount;		/* new thread cache */
	u_int32_t	gd_reqflags;		/* (see note above) */
	union sysunion	*gd_freesysun;		/* free syscall messages */
	TAILQ_HEAD(,thread) gd_tdallq;		/* all threads */
	TAILQ_HEAD(,thread) gd_tdfreeq;		/* new thread cache */
	TAILQ_HEAD(,thread) gd_tdrunq[32];	/* runnable threads */
	u_int32_t	gd_runqmask;		/* which queues? */
	u_int		gd_cpuid;
	u_int		gd_other_cpus;		/* mask of 'other' cpus */
	struct timeval	gd_stattv;
	int		gd_intr_nesting_level;	/* (for interrupts) */
	int		gd_psticks;		/* profile kern/kern_clock.c */
	int		gd_psdiv;		/* profile kern/kern_clock.c */
	struct vmmeter	gd_cnt;
	struct lwkt_ipiq *gd_ipiq;
	short		gd_upri;		/* userland scheduler helper */
	short		gd_unused01;
	struct thread	gd_schedthread;		/* userland scheduler helper */
	struct thread	gd_idlethread;
	SLGlobalData	gd_slab;		/* slab allocator */
	int		gd_vme_kdeficit;	/* vm_map_entry reservation */
	int		gd_vme_avail;		/* vm_map_entry reservation */
	struct vm_map_entry *gd_vme_base;	/* vm_map_entry reservation */
	/* extended by <machine/pcpu.h> */
};

typedef struct globaldata *globaldata_t;

#define RQB_IPIQ	0
#define RQB_INTPEND	1
#define RQB_AST_OWEUPC	2
#define RQB_AST_SIGNAL	3
#define RQB_AST_RESCHED	4

#define RQF_IPIQ	(1 << RQB_IPIQ)
#define RQF_INTPEND	(1 << RQB_INTPEND)
#define RQF_AST_OWEUPC	(1 << RQB_AST_OWEUPC)
#define RQF_AST_SIGNAL	(1 << RQB_AST_SIGNAL)
#define RQF_AST_RESCHED	(1 << RQB_AST_RESCHED)
#define RQF_AST_MASK	(RQF_AST_OWEUPC|RQF_AST_SIGNAL|RQF_AST_RESCHED)

#endif

#ifdef _KERNEL
struct globaldata *globaldata_find(int cpu);
#endif

#endif
