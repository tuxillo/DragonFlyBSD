/*-
 * Copyright (c) 2026 The DragonFly Project
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LINUXKPI_SYS_CPUSET_H_
#define _LINUXKPI_SYS_CPUSET_H_

/*
 * DragonFly cpuset compatibility wrapper for LinuxKPI.
 *
 * DragonFly has cpumask_t defined in machine/cpumask.h (via sys/cpumask.h)
 * and cpuset_t typedef'd in sys/sched.h.
 *
 * Important: Include machine/cpumask.h first to get the CPUMASK_* macros
 * that we need for the CPU_* wrapper macros.
 */

#ifdef __DragonFly__

/*
 * Include machine/cpumask.h directly to get:
 * - __cpumask_t struct definition
 * - CPUMASK_* macros (CPUMASK_ASSZERO, CPUMASK_ORBIT, etc.)
 * - BSFCPUMASK, etc.
 */
#include <machine/cpumask.h>

/*
 * sys/cpumask.h provides the cpumask_t typedef from __cpumask_t.
 * We need this before including sched.h which uses cpumask_t.
 */
#include <sys/cpumask.h>

/*
 * cpuset_t - FreeBSD compatibility typedef.
 *
 * DragonFly's sys/sched.h also typedefs cpuset_t = cpumask_t, but that
 * header has many dependencies and may not always be included first.
 * We always provide the typedef here to ensure it's available.
 *
 * Note: Multiple identical typedefs are allowed in C, so this is safe
 * even if sched.h is later included.
 */
typedef cpumask_t cpuset_t;	/* FreeBSD compatibility */
typedef cpumask_t cpu_set_t;	/* Linux compatibility */

/* Maximum number of CPUs - DragonFly x86_64 supports up to 256 */
#ifndef CPU_MAXSIZE
#define CPU_MAXSIZE	256
#endif

#ifndef CPU_SETSIZE
#define CPU_SETSIZE	CPU_MAXSIZE
#endif

/*
 * FreeBSD-style CPU_* macros for kernel code.
 * The sys/cpumask.h definitions are only for userspace (#ifndef _KERNEL).
 * In kernel, we need to provide our own using CPUMASK_* macros.
 */
#ifdef _KERNEL

#ifndef CPU_ZERO
#define CPU_ZERO(cpuset)	CPUMASK_ASSZERO(*(cpuset))
#endif

#ifndef CPU_SET
#define CPU_SET(cpu, cpuset)	CPUMASK_ORBIT(*(cpuset), (cpu))
#endif

#ifndef CPU_CLR
#define CPU_CLR(cpu, cpuset)	CPUMASK_NANDBIT(*(cpuset), (cpu))
#endif

#ifndef CPU_ISSET
#define CPU_ISSET(cpu, cpuset)	CPUMASK_TESTBIT(*(cpuset), (cpu))
#endif

#ifndef CPU_COPY
#define CPU_COPY(src, dst)	do { *(dst) = *(src); } while(0)
#endif

#ifndef CPU_FSET
#define CPU_FSET(cpuset)	CPUMASK_ASSALLONES(*(cpuset))
#endif

/*
 * CPU_COUNT - count number of bits set in cpuset.
 * DragonFly cpumask_t has 4 x 64-bit words.
 * Use bitcount64 to avoid __popcountdi2 linker errors when -mno-popcnt is used.
 */
#ifndef CPU_COUNT
static __inline int
_lkpi_cpu_count(const cpuset_t *cpuset)
{
	return bitcount64(cpuset->ary[0]) +
	       bitcount64(cpuset->ary[1]) +
	       bitcount64(cpuset->ary[2]) +
	       bitcount64(cpuset->ary[3]);
}
#define CPU_COUNT(cpuset)	_lkpi_cpu_count(cpuset)
#endif

#ifndef CPU_EMPTY
#define CPU_EMPTY(cpuset)	CPUMASK_TESTZERO(*(cpuset))
#endif

#ifndef CPU_CMP
#define CPU_CMP(s1, s2)		(!CPUMASK_CMPMASKEQ(*(s1), *(s2)))
#endif

/*
 * CPU_FOREACH - iterate over all possible CPUs.
 * Note: ncpus is declared in sys/systm.h
 */
#ifndef CPU_FOREACH
#define CPU_FOREACH(cpu) \
	for ((cpu) = 0; (cpu) < ncpus; (cpu)++)
#endif

/*
 * CPU_FOREACH_ISSET - iterate over CPUs set in a cpuset.
 */
#ifndef CPU_FOREACH_ISSET
#define CPU_FOREACH_ISSET(cpu, cpuset) \
	for ((cpu) = 0; (cpu) < ncpus; (cpu)++) \
		if (CPUMASK_TESTBIT(*(cpuset), (cpu)))
#endif

/*
 * CPU_FIRST - find first set bit in cpuset.
 * Uses DragonFly's BSFCPUMASK macro.
 */
#ifndef CPU_FIRST
#define CPU_FIRST(cpuset)	(CPUMASK_TESTZERO(*(cpuset)) ? -1 : BSFCPUMASK(*(cpuset)))
#endif

/* For compatibility with FreeBSD's setfirst */
#ifndef setfirst
#define setfirst(cpuset)	CPU_FIRST(cpuset)
#endif

#endif /* _KERNEL */

#else /* !__DragonFly__ - FreeBSD */

/* On FreeBSD, include the native sys/_cpuset.h */
#include_next <sys/cpuset.h>

#endif /* __DragonFly__ */

#endif /* _LINUXKPI_SYS_CPUSET_H_ */
