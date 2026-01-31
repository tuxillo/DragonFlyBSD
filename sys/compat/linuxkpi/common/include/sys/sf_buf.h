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

#ifndef _SYS_SF_BUF_H_
#define _SYS_SF_BUF_H_

/* DragonFly sendfile buffer compatibility for LinuxKPI (FreeBSD sf_buf) */

#include <sys/types.h>
#include <vm/vm.h>

/*
 * sf_buf (sendfile buffer) provides a way to map file pages into the kernel
 * for zero-copy socket sendfile operations.
 * DragonFly has different mechanisms, so this is a compatibility stub.
 *
 * IMPORTANT: This file uses DragonFly's native malloc API directly
 * (not the LinuxKPI kmalloc/kfree macros) to avoid conflicts when
 * linux/slab.h redefines kmalloc/kfree with different signatures.
 */

/* Use native DragonFly allocator to avoid LinuxKPI macro conflicts */
static __inline void *
sf_buf_native_alloc(size_t size, int flags)
{
#ifdef SLAB_DEBUG
    return _kmalloc_debug(size, M_TEMP, flags, __FILE__, __LINE__);
#else
    return _kmalloc(size, M_TEMP, flags);
#endif
}

static __inline void
sf_buf_native_free(void *addr)
{
    _kfree(addr, M_TEMP);
}

/* sf_buf structure - minimal stub */
struct sf_buf {
    vm_offset_t kva;        /* Kernel virtual address */
    vm_page_t page;         /* Page pointer */
    int ref_count;          /* Reference count */
};

/* sf_buf flags */
#define SFB_CATCH   0x0001  /* May sleep for memory */
#define SFB_NOCATCH 0x0002  /* Don't sleep for memory */
#define SFB_CPUPRIVATE  0x0004  /* CPU private mapping */
#define SFB_NOWAIT SFB_NOCATCH

/* sf_buf cache limits */
#ifndef NSFBUFS
#define NSFBUFS     128     /* Default number of sf_bufs */
#endif

/* Maximum reference count */
#define SFBUF_HIWAT 256

/* sf_buf allocation - stub */
static __inline struct sf_buf *
sf_buf_alloc(struct vm_page *m, int flags)
{
    struct sf_buf *sf;
    
    sf = sf_buf_native_alloc(sizeof(struct sf_buf), (flags & SFB_NOCATCH) ? M_NOWAIT : M_WAITOK);
    if (sf) {
        sf->kva = 0;
        sf->page = m;
        sf->ref_count = 1;
    }
    return sf;
}

/* sf_buf deallocation - stub */
static __inline void
sf_buf_free(struct sf_buf *sf)
{
    if (sf) {
        sf_buf_native_free(sf);
    }
}

/* Get kernel virtual address from sf_buf - stub */
static __inline vm_offset_t
sf_buf_kva(struct sf_buf *sf)
{
    return sf ? sf->kva : 0;
}

/* Get page pointer from sf_buf */
static __inline vm_page_t
sf_buf_page(struct sf_buf *sf)
{
    return sf ? sf->page : NULL;
}

/* sf_buf cache management - stubs */
static __inline void
sf_buf_shrink(void)
{
}

static __inline void
sf_buf_clear(void)
{
}

static __inline int
sf_buf_max(void)
{
    return NSFBUFS;
}

static __inline int
sf_buf_count(void)
{
    return 0;
}

/* System initialization - stub */
static __inline void
sf_buf_init(void)
{
}

/* Global sync for all CPUs - stub */
static __inline void
sf_buf_global_sync(void)
{
}

/* Per-CPU sf_buf cache - stub structures */
struct sf_buf_list {
    struct sf_buf *sf_bufs[NSFBUFS];
    int count;
};

static __inline struct sf_buf_list *
sf_buf_cache_get(void)
{
    return NULL;
}

/* Reference counting helpers */
static __inline void
sf_buf_ref(struct sf_buf *sf)
{
    if (sf)
        sf->ref_count++;
}

static __inline int
sf_buf_unref(struct sf_buf *sf)
{
    if (sf) {
        sf->ref_count--;
        return sf->ref_count;
    }
    return 0;
}

/* Direct I/O buffer handling - stubs */
static __inline struct sf_buf *
sf_buf_dio_alloc(vm_page_t m, int flags)
{
    return sf_buf_alloc(m, flags);
}

static __inline void
sf_buf_dio_free(struct sf_buf *sf)
{
    sf_buf_free(sf);
}

/* CPU private sf_buf - stub */
static __inline struct sf_buf *
sf_buf_cpuprivate_alloc(vm_page_t m, int flags)
{
    return sf_buf_alloc(m, flags);
}

static __inline void
sf_buf_cpuprivate_free(struct sf_buf *sf)
{
    sf_buf_free(sf);
}

#endif /* _SYS_SF_BUF_H_ */
