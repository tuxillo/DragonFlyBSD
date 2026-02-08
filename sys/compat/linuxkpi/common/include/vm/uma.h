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
#include <sys/objcache.h>
#include <sys/param.h>

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
    /* kmalloc() macro takes 2 args (size, flags) and adds M_TEMP internally.
     * It expands to _kmalloc_debug when SLAB_DEBUG is enabled.
     */
    return kmalloc(size, flags);
}

static __inline void
uma_native_free(void *addr)
{
    _kfree(addr, M_TEMP);
}

typedef int uma_ctor_t(void *mem, int size, void *arg, int flags);
typedef void uma_dtor_t(void *mem, int size, void *arg);
typedef int uma_init_t(void *mem, int size, int flags);
typedef void uma_fini_t(void *mem, int size);

/*
 * UMA zone type - maps to DragonFly's allocator and objcache.
 * FreeBSD defines uma_zone_t as a pointer: typedef struct uma_zone *uma_zone_t
 * We match that convention for compatibility.
 */
struct uma_zone {
    const char *name;
    size_t size;
    size_t align;
    uint32_t flags;
    uma_ctor_t *ctor;
    uma_dtor_t *dtor;
    uma_init_t *zinit;
    uma_fini_t *zfini;
    struct objcache *oc;
    void **reserve_items;
    uint32_t reserve_count;
    uint32_t reserve_target;
    uint64_t allocs;
    uint64_t frees;
    uint64_t fails;
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

static __inline void *
uma_alloc_aligned(uma_zone_t zone, int flags)
{
    size_t mask;
    size_t size;
    uintptr_t base;
    uintptr_t addr;
    uintptr_t aligned;
    void **slot;

    mask = zone->align;
    if (mask == 0)
        return uma_native_alloc(zone->size, flags);

    size = zone->size + mask + sizeof(void *);
    base = (uintptr_t)uma_native_alloc(size, flags);
    if (base == 0)
        return NULL;
    addr = base + sizeof(void *);
    aligned = (addr + mask) & ~mask;
    slot = (void **)aligned;
    slot[-1] = (void *)base;
    return (void *)aligned;
}

static __inline void
uma_free_aligned(uma_zone_t zone, void *ptr)
{
    uintptr_t base;

    if (zone->align == 0) {
        uma_native_free(ptr);
        return;
    }

    base = (uintptr_t)((void **)ptr)[-1];
    uma_native_free((void *)base);
}

static __inline void *
uma_objcache_alloc(void *arg, int ocflags)
{
    uma_zone_t zone = arg;
    int flags = (ocflags & M_NOWAIT) ? M_NOWAIT : M_WAITOK;

    return uma_alloc_aligned(zone, flags);
}

static __inline void
uma_objcache_free(void *obj, void *arg)
{
    uma_zone_t zone = arg;

    uma_free_aligned(zone, obj);
}

static __inline void
uma_zone_put_raw(uma_zone_t zone, void *item)
{
    if (zone->reserve_count < zone->reserve_target &&
        zone->reserve_items != NULL) {
        zone->reserve_items[zone->reserve_count++] = item;
    } else if (zone->oc != NULL) {
        objcache_put(zone->oc, item);
    } else {
        uma_free_aligned(zone, item);
    }
}

/* UMA zone creation/destruction */
static __inline uma_zone_t
uma_zcreate(const char *name, size_t size, void *ctor, void *dtor, 
            void *zinit, void *zfini, int align, uint32_t flags)
{
    uma_zone_t zone;
    zone = uma_native_alloc(sizeof(struct uma_zone), M_WAITOK | M_ZERO);
    if (zone != NULL) {
        zone->name = name;
        zone->size = size;
        zone->align = align;
        zone->flags = flags;
        zone->ctor = (uma_ctor_t *)ctor;
        zone->dtor = (uma_dtor_t *)dtor;
        zone->zinit = (uma_init_t *)zinit;
        zone->zfini = (uma_fini_t *)zfini;
        zone->oc = objcache_create(name, 0, 0, NULL, NULL, zone,
            uma_objcache_alloc, uma_objcache_free, zone);
    }
    return zone;
}

static __inline void
uma_zdestroy(uma_zone_t zone)
{
    uint32_t i;

    if (zone == NULL)
        return;

    for (i = 0; i < zone->reserve_count; i++) {
        if (zone->zfini != NULL)
            zone->zfini(zone->reserve_items[i], (int)zone->size);
        uma_free_aligned(zone, zone->reserve_items[i]);
    }
    if (zone->reserve_items != NULL)
        uma_native_free(zone->reserve_items);
    if (zone->oc != NULL)
        objcache_destroy(zone->oc);
    uma_native_free(zone);
}

static __inline void *
uma_zalloc_arg(uma_zone_t zone, void *arg, int flags)
{
    void *item;
    int ocflags;

    if (zone == NULL)
        return NULL;

    if ((flags & M_NOWAIT) && zone->reserve_count != 0) {
        item = zone->reserve_items[--zone->reserve_count];
    } else {
        ocflags = (flags & M_NOWAIT) ? M_NOWAIT : M_WAITOK;
        item = zone->oc != NULL ? objcache_get(zone->oc, ocflags)
                                : uma_alloc_aligned(zone, ocflags);
    }

    if (item == NULL) {
        zone->fails++;
        return NULL;
    }

    if ((flags & M_ZERO) != 0)
        bzero(item, zone->size);

    if (zone->zinit != NULL && zone->zinit(item, (int)zone->size, flags) != 0) {
        uma_zone_put_raw(zone, item);
        zone->fails++;
        return NULL;
    }

    if (zone->ctor != NULL && zone->ctor(item, (int)zone->size, arg, flags) != 0) {
        if (zone->zfini != NULL)
            zone->zfini(item, (int)zone->size);
        uma_zone_put_raw(zone, item);
        zone->fails++;
        return NULL;
    }

    zone->allocs++;
    return item;
}

static __inline void
uma_zfree_arg(uma_zone_t zone, void *item, void *arg)
{
    if (zone == NULL || item == NULL)
        return;

    if (zone->dtor != NULL)
        zone->dtor(item, (int)zone->size, arg);
    if (zone->zfini != NULL)
        zone->zfini(item, (int)zone->size);

    uma_zone_put_raw(zone, item);
    zone->frees++;
}

static __inline void *
uma_zalloc(uma_zone_t zone, int flags)
{
    return uma_zalloc_arg(zone, NULL, flags);
}

static __inline void
uma_zfree(uma_zone_t zone, void *item)
{
    uma_zfree_arg(zone, item, NULL);
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

static __inline void
uma_zone_reserve(uma_zone_t zone, int n)
{
    if (zone == NULL || n <= 0)
        return;
    if (zone->reserve_target < (uint32_t)n)
        zone->reserve_target = n;
}

/* UMA zone stats - stubs */
static __inline uint64_t
uma_zone_get_cur(uma_zone_t zone)
{
    return zone != NULL ? (zone->allocs - zone->frees) : 0;
}

static __inline uint64_t
uma_zone_get_max(uma_zone_t zone)
{
    return 0;
}

static __inline uint64_t
uma_zone_get_allocs(uma_zone_t zone)
{
    return zone != NULL ? zone->allocs : 0;
}

static __inline uint64_t
uma_zone_get_frees(uma_zone_t zone)
{
    return zone != NULL ? zone->frees : 0;
}

static __inline uint64_t
uma_zone_get_fail(uma_zone_t zone)
{
    return zone != NULL ? zone->fails : 0;
}

static __inline void
uma_prealloc(uma_zone_t zone, int n)
{
    uint32_t i;
    uint32_t target;
    void *item;

    if (zone == NULL || n <= 0)
        return;

    if (zone->reserve_target < (uint32_t)n)
        zone->reserve_target = n;
    target = zone->reserve_target;

    if (zone->reserve_items == NULL) {
        zone->reserve_items = uma_native_alloc(sizeof(void *) * target,
            M_WAITOK | M_ZERO);
    } else if (zone->reserve_count < target) {
        zone->reserve_items = realloc(zone->reserve_items,
            sizeof(void *) * target, M_TEMP, M_WAITOK | M_ZERO);
    }

    for (i = zone->reserve_count; i < target; i++) {
        item = uma_alloc_aligned(zone, M_WAITOK);
        if (item == NULL)
            break;
        zone->reserve_items[zone->reserve_count++] = item;
    }
}

/* System zone for general allocations */
#define uma_zone_get_obj(zone, size, flags) uma_zalloc(zone, flags)

#endif /* _VM_UMA_H_ */
