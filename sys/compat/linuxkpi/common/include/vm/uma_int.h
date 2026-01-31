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

#ifndef _VM_UMA_INT_H_
#define _VM_UMA_INT_H_

/* DragonFly UMA internal definitions for LinuxKPI (FreeBSD UMA) */

#include <sys/types.h>
#include <vm/uma.h>

/*
 * UMA (Universal Memory Allocator) internal structures.
 * This header exposes implementation details needed by some LinuxKPI code.
 */

/* Slab structure - minimal stub */
struct uma_slab {
    struct uma_slab *us_next;       /* Next slab in list */
    void *us_data;                  /* Start of data */
    int us_freecount;               /* Number of free items */
    int us_flags;                   /* Flags */
};

/* UMA slab flags */
#define UMA_SLAB_OFFPAGE    0x01    /* Slab metadata is off-page */
#define UMA_SLAB_BOOT       0x02    /* Slab allocated from boot memory */
#define UMA_SLAB_MALLOC     0x04    /* Slab uses malloc */

/* UMA bucket structure - minimal stub */
struct uma_bucket {
    struct uma_bucket *ub_next;     /* Next bucket in list */
    void **ub_ptr;                  /* Pointer to items */
    int ub_count;                   /* Number of items */
    int ub_size;                    /* Max items in bucket */
};

/* UMA keg (bucket supply) structure - minimal stub */
struct uma_keg {
    char *uk_name;                  /* Zone name */
    size_t uk_size;                 /* Item size */
    size_t uk_align;                /* Alignment */
    uint32_t uk_flags;              /* Keg flags */
    int uk_pages;                   /* Pages per slab */
    struct uma_slab *uk_partial;    /* Partially allocated slabs */
    struct uma_slab *uk_full;       /* Full slabs */
    struct uma_slab *uk_free;       /* Empty slabs */
    struct uma_bucket *uk_buckets;  /* Bucket list */
    int uk_slabsize;                /* Slab size in bytes */
    int uk_ppera;                   /* Pages per allocation */
};

/* UMA zone structure extended with internal fields */
struct uma_zone_ext {
    struct uma_zone base;           /* Base zone structure */
    struct uma_keg *uz_keg;         /* Backing keg */
    uint32_t uz_flags;              /* Zone flags */
    int uz_cursor;                  /* Allocation cursor */
    struct uma_bucket *uz_buckets;  /* Per-CPU buckets */
};

/* Keg flags */
#define UMA_KFLAG_FULL          0x00000001
#define UMA_KFLAG_CACHEONLY     0x00000002
#define UMA_KFLAG_NOBUCKET      0x00000004
#define UMA_KFLAG_ALIGNANY      0x00000008
#define UMA_KFLAG_RECLAIM       0x00000010

/* Internal allocation functions - stubs */
static __inline void *
uma_slab_alloc(struct uma_zone *zone, struct uma_slab **slab_out)
{
    return uma_zalloc(zone, M_NOWAIT);
}

static __inline void
uma_slab_free(struct uma_zone *zone, struct uma_slab *slab, void *item)
{
    uma_zfree(zone, item);
}

/* Bucket operations - stubs */
static __inline struct uma_bucket *
uma_bucket_alloc(struct uma_zone *zone, int size)
{
    return NULL;
}

static __inline void
uma_bucket_free(struct uma_zone *zone, struct uma_bucket *bucket)
{
}

static __inline int
uma_bucket_get(struct uma_bucket *bucket, void **items, int max)
{
    return 0;
}

static __inline void
uma_bucket_put(struct uma_bucket *bucket, void **items, int count)
{
}

/* Keg operations - stubs */
static __inline struct uma_keg *
uma_kcreate(const char *name, size_t size, size_t align,
            uint32_t flags, void *uminit, void *fini,
            int pages, int ppera)
{
    return NULL;
}

static __inline void
uma_kdestroy(struct uma_keg *keg)
{
}

static __inline void
uma_keg_reclaim(struct uma_keg *keg, int flags)
{
}

/* Zone/keg linking - stub */
static __inline void
uma_zone_set_kctx(struct uma_zone *zone, struct uma_keg *keg)
{
}

static __inline struct uma_keg *
uma_zone_get_kctx(struct uma_zone *zone)
{
    return NULL;
}

/* Slab allocation from keg - stub */
static __inline struct uma_slab *
uma_keg_alloc_slab(struct uma_keg *keg, struct uma_zone *zone, int wait)
{
    return NULL;
}

static __inline void
uma_keg_free_slab(struct uma_keg *keg, struct uma_slab *slab)
{
}

/* Reclaim callback type */
typedef int uma_reclaim_func_t(struct uma_zone *zone, int flags);

/* Set reclaim callback - stub */
static __inline void
uma_zone_set_reclaim(struct uma_zone *zone, uma_reclaim_func_t *func)
{
}

/* Drain functions */
static __inline void
uma_zone_drain(struct uma_zone *zone)
{
}

static __inline void
uma_keg_drain(struct uma_keg *keg)
{
}

static __inline void
uma_zone_drain_all(int waitok)
{
}

/* Debugging helpers - stubs */
static __inline void
uma_print_zone(struct uma_zone *zone)
{
}

static __inline void
uma_print_keg(struct uma_keg *keg)
{
}

static __inline void
uma_print_stats(void)
{
}

/* Bootstrap allocation - stub */
static __inline void *
uma_bootalloc(size_t size)
{
    return NULL;
}

static __inline void
uma_bootfree(void *ptr, size_t size)
{
}

/* UMA small allocation - stub */
static __inline void *
uma_small_alloc(size_t size, int wait)
{
    return NULL;
}

static __inline void
uma_small_free(void *ptr, size_t size)
{
}

/* Alignment helpers */
#define UMA_ALIGN_PTR   (sizeof(void *))
#define UMA_ALIGN_LONG  (sizeof(long))
#define UMA_ALIGN_INT   (sizeof(int))
#define UMA_ALIGN_SHORT (sizeof(short))
#define UMA_ALIGN_CHAR  (sizeof(char))
#define UMA_ALIGN_1     1
#define UMA_ALIGN_2     2
#define UMA_ALIGN_4     4
#define UMA_ALIGN_8     8
#define UMA_ALIGN_16    16
#define UMA_ALIGN_32    32
#define UMA_ALIGN_64    64
#define UMA_ALIGN_128   128
#define UMA_ALIGN_256   256
#define UMA_ALIGN_512   512
#define UMA_ALIGN_1024  1024
#define UMA_ALIGN_2048  2048
#define UMA_ALIGN_4096  4096

/* Slab size calculation - stub */
static __inline int
uma_calc_slab(size_t size, int align)
{
    return PAGE_SIZE;
}

#endif /* _VM_UMA_INT_H_ */
