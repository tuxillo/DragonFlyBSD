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

#ifndef _SYS_DOMAINSET_H_
#define _SYS_DOMAINSET_H_

/*
 * DragonFly domainset compatibility for LinuxKPI (FreeBSD NUMA)
 *
 * DragonFly doesn't have NUMA support. This header provides stub
 * implementations that treat the system as a single memory domain.
 */

#include <sys/cpuset.h>
#include <sys/malloc.h>

/*
 * MAXMEMDOM - Maximum number of memory domains (NUMA nodes).
 * FreeBSD uses this for NUMA. DragonFly doesn't have NUMA, so we use 1.
 */
#ifndef MAXMEMDOM
#define MAXMEMDOM	1
#endif

/* Domain set is just a cpuset in DragonFly (no NUMA support) */
typedef cpuset_t domainset_t;

/* Forward declare struct domainset for pointers */
struct domainset;

/*
 * Use native DragonFly allocator to avoid LinuxKPI macro conflicts.
 * This file may be included before or after linux/slab.h redefines
 * kmalloc/kfree with different signatures.
 */
static __inline void *
domainset_native_alloc(size_t size, int flags)
{
#ifdef SLAB_DEBUG
    return _kmalloc_debug(size, M_TEMP, flags, __FILE__, __LINE__);
#else
    return _kmalloc(size, M_TEMP, flags);
#endif
}

static __inline void
domainset_native_free(void *addr)
{
    _kfree(addr, M_TEMP);
}

/* Basic domainset macros - map to cpuset operations */
#define DOMAINSET_NULL		NULL
#define DOMAINSET_SIZE(n)	sizeof(cpuset_t)
#define DOMAINSET_SET(n, p)	CPU_SET(n, p)
#define DOMAINSET_CLR(n, p)	CPU_CLR(n, p)
#define DOMAINSET_ISSET(n, p)	CPU_ISSET(n, p)
#define DOMAINSET_COPY(f, t)	CPU_COPY(f, t)
#define DOMAINSET_ZERO(p)	CPU_ZERO(p)
#define DOMAINSET_FSET(p)	CPU_FSET(p)
#define DOMAINSET_PCNT(p)	CPU_COUNT(p)
#define DOMAINSET_SUB(s1, s2, d) CPU_SUB(s1, s2, d)
#define DOMAINSET_CMP(s1, s2)	CPU_CMP(s1, s2)
#define DOMAINSET_VALID(mem, idx) 1

#define DOMAINSET_FORSUB(i, mask) CPU_FOREACH_ISSET(i, mask)

/* Domain set policy constants */
#define DOMAINSET_POLICY_RR		0
#define DOMAINSET_POLICY_FIRSTFIT	1
#define DOMAINSET_POLICY_FIXED		2
#define DOMAINSET_POLICY_PREF		3
#define DOMAINSET_POLICY_MAX		4

/*
 * DOMAINSET_RR() and DOMAINSET_PREF() - Get default domain set policies.
 * DragonFly doesn't have NUMA support, so we return NULL for all
 * domainset requests. Allocations will use the default policy.
 */
#define DOMAINSET_RR()		((struct domainset *)NULL)
#define DOMAINSET_PREF(node)	((struct domainset *)NULL)

/*
 * Domainset inline functions.
 * These provide FreeBSD-compatible APIs that do nothing useful on DragonFly.
 */

static __inline domainset_t *
domainset_alloc(void)
{
    domainset_t *ds;
    ds = (domainset_t *)domainset_native_alloc(sizeof(domainset_t), M_WAITOK);
    if (ds)
        DOMAINSET_ZERO(ds);
    return ds;
}

static __inline void
domainset_free(domainset_t *ds)
{
    if (ds)
        domainset_native_free(ds);
}

static __inline void
domainset_copy(domainset_t *src, domainset_t *dst)
{
    DOMAINSET_COPY(src, dst);
}

static __inline int
domainset_equal(domainset_t *a, domainset_t *b)
{
    return DOMAINSET_CMP(a, b) == 0;
}

static __inline int
domainset_empty(domainset_t *ds)
{
    return CPU_EMPTY(ds);
}

static __inline int
domainset_issingleton(domainset_t *ds)
{
    return CPU_SINGLETON(ds);
}

/* Get the first domain in a domainset */
static __inline int
domainset_first_domain(domainset_t *ds)
{
    if (ds == NULL || CPU_EMPTY(ds))
        return (0);  /* Default to domain 0 */
    return CPU_FIRST(ds);
}

/* Get the next domain after 'prev' in a domainset */
static __inline int
domainset_next_domain(int prev, domainset_t *ds)
{
    if (ds == NULL)
        return (-1);
    return CPU_NEXT(prev, ds);
}

/* Get the first valid domain, preferring 'prefer' if valid */
static __inline int
domainset_firstvalid(int prefer, domainset_t *mask)
{
    if (mask == NULL)
        return (0);
    if (prefer >= 0 && DOMAINSET_ISSET(prefer, mask))
        return (prefer);
    return domainset_first_domain(mask);
}

/* Get a CPU/domain from a mask based on policy */
static __inline int
domainset_getcpu(domainset_t *mask, int prefer, int policy)
{
    (void)policy;  /* Policy ignored - no NUMA */
    return domainset_firstvalid(prefer, mask);
}

/*
 * Macros for backward compatibility with code using domainset_first/next.
 * Use function versions with different names to avoid macro/function conflicts.
 */
#define domainset_first(ds)		domainset_first_domain(ds)
#define domainset_next(prev, ds)	domainset_next_domain(prev, ds)

#endif /* _SYS_DOMAINSET_H_ */
