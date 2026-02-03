/*-
 * Copyright (c) 2026 The DragonFly Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions and the following
 *    disclaimer.
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
 */

#ifndef _SYS_SF_BUF_H_
#define _SYS_SF_BUF_H_

/*
 * DragonFly sendfile buffer compatibility for LinuxKPI (FreeBSD sf_buf API).
 *
 * On platforms with a direct map (DMAP), sf_bufs are zero-allocation and
 * simply use the direct map for KVA.
 */

#include <sys/types.h>
#include <vm/vm_page.h>
#include <machine/vmparam.h>

#ifndef PMAP_HAS_DMAP
#ifdef PHYS_TO_DMAP
#define PMAP_HAS_DMAP 1
#else
#define PMAP_HAS_DMAP 0
#endif
#endif

/* FreeBSD-compatible flags used by LinuxKPI call sites */
#define SFB_CATCH       1
#define SFB_CPUPRIVATE  2
#define SFB_DEFAULT     0
#define SFB_NOWAIT      4

#if PMAP_HAS_DMAP

struct sf_buf;

static __inline struct sf_buf *
sf_buf_alloc(vm_page_t m, int flags)
{
	(void)flags;
	return ((struct sf_buf *)m);
}

static __inline void
sf_buf_free(struct sf_buf *sf)
{
	(void)sf;
}

static __inline void
sf_buf_ref(struct sf_buf *sf)
{
	(void)sf;
}

static __inline vm_offset_t
sf_buf_kva(struct sf_buf *sf)
{
	return (PHYS_TO_DMAP(VM_PAGE_TO_PHYS((vm_page_t)sf)));
}

static __inline vm_page_t
sf_buf_page(struct sf_buf *sf)
{
	return ((vm_page_t)sf);
}

#else

#define __dfly_sf_buf_alloc sf_buf_alloc
#define __dfly_sf_buf_free sf_buf_free
#define __dfly_sf_buf_ref sf_buf_ref
#include <sys/sfbuf.h>
#undef sf_buf_alloc
#undef sf_buf_free
#undef sf_buf_ref

static __inline struct sf_buf *
sf_buf_alloc(vm_page_t m, int flags)
{
	(void)flags;
	return (__dfly_sf_buf_alloc(m));
}

static __inline void
sf_buf_free(struct sf_buf *sf)
{
	__dfly_sf_buf_free(sf);
}

static __inline void
sf_buf_ref(struct sf_buf *sf)
{
	__dfly_sf_buf_ref(sf);
}

#endif /* PMAP_HAS_DMAP */

#endif /* _SYS_SF_BUF_H_ */
