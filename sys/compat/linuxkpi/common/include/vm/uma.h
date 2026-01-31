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

#ifndef _VM_UMA_H_
#define _VM_UMA_H_

/* DragonFly UMA (Universal Memory Allocator) compatibility for LinuxKPI */

#include <sys/types.h>
#include <sys/malloc.h>

/* 
 * UMA is FreeBSD's slab allocator. DragonFly uses a different allocator
 * (objcache and kmalloc), so we provide compatibility wrappers.
 *
 * IMPORTANT: This file uses DragonFly's native malloc API directly
 * (not the LinuxKPI kmalloc/kfree macros) to avoid conflicts when
 * linux/slab.h redefines kmalloc/kfree with different signatures.
 */

/* Use a local helper to call DragonFly's native allocator */
static __inline void *
uma_native_alloc(size_t size, int flags)
{
#ifdef SLAB_DEBUG
    return _kmalloc_debug(size, M_TEMP, flags, __FILE__, __LINE__);
#else
    return _kmalloc(size, M_TEMP, flags);
#endif
}

static __inline void
uma_native_free(void *addr)
{
    _kfree(addr, M_TEMP);
}

/*
 * UMA zone type - maps to DragonFly's allocator.
 * FreeBSD defines uma_zone_t as a pointer: typedef struct uma_zone *uma_zone_t
 * We match that convention for compatibility.
 */
struct uma_zone {
    const char *name;
    size_t size;
    size_t align;
};
typedef struct uma_zone *uma_zone_t;

/* UMA zone flags */
#define UMA_ZONE_FIXED_SIZE 0x0001
#define UMA_ZONE_ZINIT 0x0002
#define UMA_ZONE_ZFINI 0x0004
#define UMA_ZONE_MALLOC 0x0008
#define UMA_ZONE_NOFREE 0x0010
#define UMA_ZONE_MTXCLASS 0x0020
#define UMA_ZONE_RDY 0x0040
#define UMA_ZONE_MAXBUCKETS 0x0080
#define UMA_ZONE_CTOR 0x0100
#define UMA_ZONE_DTOR 0x0200
#define UMA_ZONE_RECLAIM 0x0400
#define UMA_ZONE_ALIGN_SHIFT 8
#define UMA_ZONE_ALIGN_MASK 0xFF00
#define UMA_ZONE_ALIGN(n) ((n) << UMA_ZONE_ALIGN_SHIFT)
#define UMA_ZONE_ALIGN_PTR (sizeof(void *) << UMA_ZONE_ALIGN_SHIFT)
#define UMA_ZONE_VM 0x10000
#define UMA_ZONE_HASH 0x20000
#define UMA_ZONE_SECONDARY 0x40000
#define UMA_ZONE_INHERIT 0x80000
#define UMA_ZONE_SMR 0x100000

/* UMA allocation flags */
#define UMA_SLAB_NOFREE 0x0001
#define UMA_SLAB_KERNEL 0x0002
#define UMA_SLAB_MALLOC 0x0004

/* UMA alignment - match FreeBSD */
#define UMA_ALIGN_PTR       (sizeof(void *) - 1)
#define UMA_ALIGN_LONG      (sizeof(long) - 1)
#define UMA_ALIGN_INT       (sizeof(int) - 1)
#define UMA_ALIGN_SHORT     (sizeof(short) - 1)
#define UMA_ALIGN_CHAR      0
#define UMA_ALIGN_CACHE     63  /* typical cache line - 1 */

/* UMA zone creation/destruction - stub implementations */
static __inline uma_zone_t
uma_zcreate(const char *name, size_t size, void *ctor, void *dtor, 
            void *zinit, void *zfini, int align, uint32_t flags)
{
    uma_zone_t zone;
    /* Use native allocator helper to avoid LinuxKPI macro conflicts */
    zone = uma_native_alloc(sizeof(struct uma_zone), M_WAITOK | M_ZERO);
    if (zone != NULL) {
        zone->name = name;
        zone->size = size;
        zone->align = align;
    }
    return zone;
}

static __inline void
uma_zdestroy(uma_zone_t zone)
{
    if (zone != NULL)
        uma_native_free(zone);
}

/* UMA allocation - use native DragonFly malloc */
static __inline void *
uma_zalloc(uma_zone_t zone, int flags)
{
    if (zone == NULL)
        return NULL;
    return uma_native_alloc(zone->size, (flags & M_NOWAIT) ? M_NOWAIT : M_WAITOK);
}

static __inline void *
uma_zalloc_arg(uma_zone_t zone, void *arg, int flags)
{
    return uma_zalloc(zone, flags);
}

/* UMA free - use native DragonFly kfree */
static __inline void
uma_zfree(uma_zone_t zone, void *item)
{
    if (item != NULL)
        uma_native_free(item);
}

static __inline void
uma_zfree_arg(uma_zone_t zone, void *item, void *arg)
{
    uma_zfree(zone, item);
}

/* uma_zcache_create - FreeBSD function for creating a cache-backed UMA zone */
static __inline uma_zone_t
uma_zcache_create(const char *name, int size,
    void *ctor, void *dtor, void *zinit, void *zfini,
    void *import, void *release, void *arg, int flags)
{
    /* For cache zones with size == -1, use a default size */
    if (size < 0)
        size = sizeof(void *) * 8;  /* Reasonable default */
    return uma_zcreate(name, (size_t)size, ctor, dtor, zinit, zfini, 0, flags);
}

/* UMA cache operations - stubs */
#define uma_zone_set_max(zone, n)
#define uma_zone_set_warning(zone, n)
#define uma_zone_set_failsafe(zone, n)
#define uma_zone_set_align(zone, align)
#define uma_zone_set_free(zone, free_fn)
#define uma_zone_set_alloc(zone, alloc_fn)

/* UMA zone reserve - stub (just pre-allocate n items) */
#define uma_zone_reserve(zone, n)

/* UMA zone stats - stubs */
static __inline uint64_t
uma_zone_get_cur(uma_zone_t zone)
{
    return 0;
}

static __inline uint64_t
uma_zone_get_max(uma_zone_t zone)
{
    return 0;
}

static __inline uint64_t
uma_zone_get_allocs(uma_zone_t zone)
{
    return 0;
}

static __inline uint64_t
uma_zone_get_frees(uma_zone_t zone)
{
    return 0;
}

static __inline uint64_t
uma_zone_get_fail(uma_zone_t zone)
{
    return 0;
}

/* Pre-allocation - stub */
#define uma_prealloc(zone, n)

/* System zone for general allocations */
#define uma_zone_get_obj(zone, size, flags) uma_zalloc(zone, flags)

#endif /* _VM_UMA_H_ */
