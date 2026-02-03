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

#ifndef _SYS_PCPU_H_
#define _SYS_PCPU_H_

/* DragonFly per-CPU data compatibility for LinuxKPI (FreeBSD pcpu) */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/cpuset.h>
#include <sys/thread.h>

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

/* 
 * FreeBSD-style per-CPU data structure for LinuxKPI compatibility.
 * DragonFly uses different mechanisms for per-CPU data.
 */

/* Per-CPU data structure - minimal stub */
struct pcpu {
    int pc_cpuid;           /* CPU identifier */
    int pc_domain;          /* NUMA domain */
    cpuset_t pc_cpumask;    /* CPU mask for this CPU */
    void *pc_dynamic;       /* Dynamic data pointer */
};

/* Global pcpu array - initialized at runtime */
extern struct pcpu *pcpu_base;

/* PCPU pointer - returns pointer to this CPU's pcpu structure */
static __inline struct pcpu *
pcpu_find(int cpu)
{
    if (pcpu_base == NULL || cpu < 0 || cpu >= ncpus)
        return NULL;
    return (&pcpu_base[cpu]);
}

/* PCPU data access macros - compatibility with FreeBSD */
#ifdef PCPU_PTR
#undef PCPU_PTR
#endif
#ifdef PCPU_GET
#undef PCPU_GET
#endif
#ifdef PCPU_SET
#undef PCPU_SET
#endif
#ifdef PCPU_ADD
#undef PCPU_ADD
#endif
#ifdef PCPU_INC
#undef PCPU_INC
#endif
#ifdef PCPU_DEC
#undef PCPU_DEC
#endif
#define PCPU_PTR(member)        (pcpu_base != NULL ? &pcpu_base[mycpuid].member : NULL)
#define PCPU_GET(member)        (pcpu_base != NULL ? pcpu_base[mycpuid].member : 0)
#define PCPU_SET(member, val)   do { if (pcpu_base != NULL) pcpu_base[mycpuid].member = (val); } while (0)
#define PCPU_ADD(member, val)   do { if (pcpu_base != NULL) pcpu_base[mycpuid].member += (val); } while (0)
#define PCPU_INC(member)        do { if (pcpu_base != NULL) ++pcpu_base[mycpuid].member; } while (0)
#define PCPU_DEC(member)        do { if (pcpu_base != NULL) --pcpu_base[mycpuid].member; } while (0)

/* CPU ID macros */
#define PCPU_CPUNO()    mycpuid

/* CPU counter helpers - stubs */
static __inline uint64_t
pcpu_counter_fetch_add(struct pcpu *pc, uint64_t *counter, uint64_t add)
{
    return 0;
}

static __inline void
pcpu_counter_add(struct pcpu *pc, uint64_t *counter, uint64_t add)
{
}

static __inline uint64_t
pcpu_counter_read(struct pcpu *pc, uint64_t *counter)
{
    return 0;
}

static __inline uint64_t
pcpu_counter_readandclear(struct pcpu *pc, uint64_t *counter)
{
    return 0;
}

/* Per-CPU copy routines - stubs */
static __inline void
pcpu_copy(struct pcpu *dst, struct pcpu *src)
{
}

static __inline void
pcpu_zero(struct pcpu *pc)
{
}

/* Per-CPU initialization and allocation - stubs */
static __inline struct pcpu *
pcpu_alloc(void)
{
    return pcpu_base;
}

static __inline void
pcpu_free(struct pcpu *pc)
{
}

static __inline void
pcpu_init(struct pcpu *pc, int cpu, size_t size)
{
    pc->pc_cpuid = cpu;
    pc->pc_domain = 0;
    CPU_ZERO(&pc->pc_cpumask);
    CPU_SET(cpu, &pc->pc_cpumask);
    pc->pc_dynamic = NULL;
}

/* Allocation zone for pcpu data - stub */
static __inline void *
pcpu_malloc(size_t size)
{
    size_t stride;
    size_t total;

    if (size == 0)
        return NULL;

    stride = roundup2(size, CACHE_LINE_SIZE);
    total = stride * ncpus;
    return malloc(total, M_TEMP, M_WAITOK | M_ZERO);
}

static __inline void
pcpu_free_data(void *ptr)
{
    if (ptr != NULL)
        free(ptr, M_TEMP);
}

/* pcpu_for_each macro - iterate over all CPUs */
#define pcpu_foreach(pcp) \
    for ((pcp) = pcpu_find(0); (pcp) != NULL; (pcp) = pcpu_find((pcp)->pc_cpuid + 1))

/* Iterate over pcpus in a cpuset */
#define pcpu_foreach_belong(pcp, mask) \
    for (int __cpu = 0; __cpu < ncpus; __cpu++) \
        if (CPU_ISSET(__cpu, mask) && ((pcp) = pcpu_find(__cpu), 1))

/* Current CPU pcpu pointer */
#define curpcpu pcpu_find(mycpuid)

/* NUMA domain helpers */
static __inline int
pcpu_get_domain(int cpu)
{
    struct pcpu *pc = pcpu_find(cpu);
    return pc ? pc->pc_domain : 0;
}

static __inline int
pcpu_get_cpuid(struct pcpu *pc)
{
    return pc ? pc->pc_cpuid : -1;
}

/* Memory allocator for pcpu data */
#define DPCPU_ZONE  1

static __inline void *
dpcpu_alloc(size_t size, int domain)
{
    (void)domain;
    return pcpu_malloc(size);
}

static __inline void
dpcpu_free(void *ptr, size_t size)
{
    (void)size;
    pcpu_free_data(ptr);
}

#endif /* _SYS_PCPU_H_ */
