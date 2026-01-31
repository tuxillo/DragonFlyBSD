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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MACHINE_VM_H_
#define _MACHINE_VM_H_

/* DragonFly machine-specific VM definitions for LinuxKPI */

#include <sys/types.h>
#include <vm/vm.h>

/*
 * Machine-specific VM interface for LinuxKPI compatibility.
 *
 * This header provides machine-level VM definitions that may differ
 * across architectures. For DragonFly, this primarily re-exports
 * the standard VM definitions with any x86_64-specific additions.
 */

/* x86_64 specific cache line size */
#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE		64
#endif
#ifndef CACHE_LINE_SHIFT
#define CACHE_LINE_SHIFT	6
#endif

/* Page-level cache attributes for x86 PAT (Page Attribute Table) */
#ifndef _MACHINE_PAT_H_
/* PAT memory types if not already defined */
#define PAT_UNCACHEABLE		0x00
#define PAT_WRITE_COMBINING	0x01
#define PAT_WRITE_THROUGH	0x04
#define PAT_WRITE_PROTECTED	0x05
#define PAT_WRITE_BACK		0x06
#define PAT_UNCACHED		0x07
#endif

/* Memory type range definitions */
#ifndef VM_MEMATTR_DEFAULT
#define VM_MEMATTR_DEFAULT	0
#endif
#ifndef VM_MEMATTR_UNCACHEABLE
#define VM_MEMATTR_UNCACHEABLE	1
#endif
#ifndef VM_MEMATTR_WRITE_COMBINING
#define VM_MEMATTR_WRITE_COMBINING	2
#endif
#ifndef VM_MEMATTR_WRITE_THROUGH
#define VM_MEMATTR_WRITE_THROUGH	3
#endif
#ifndef VM_MEMATTR_WRITE_BACK
#define VM_MEMATTR_WRITE_BACK	4
#endif
#ifndef VM_MEMATTR_WEAKLY_ORDERED
#define VM_MEMATTR_WEAKLY_ORDERED	5
#endif

/*
 * Cacheability attributes for device memory mapping
 * These are commonly used by DRM drivers for GPU memory
 */
#ifndef VM_WC
#define VM_WC			VM_MEMATTR_WRITE_COMBINING
#endif
#ifndef VM_UNCACHED
#define VM_UNCACHED		VM_MEMATTR_UNCACHEABLE
#endif
#ifndef VM_CACHEABLE
#define VM_CACHEABLE		VM_MEMATTR_WRITE_BACK
#endif

/* Page attribute flags for machine-level operations */
#define VM_PAGE_FICTITIOUS	0x01
#define VM_PAGE_BUSY		0x02
#define VM_PAGE_WANTED		0x04
#define VM_PAGE_OBJECT		0x08
#define VM_PAGE_ACTIVE		0x10
#define VM_PAGE_INACTIVE	0x20

/*
 * Machine-specific page operations stubs
 */

/* Get page cache attribute */
static __inline int
vm_page_get_memattr(vm_page_t m)
{
	(void)m;
	return VM_MEMATTR_DEFAULT;
}

/* Set page cache attribute */
static __inline void
vm_page_set_memattr(vm_page_t m, int attr)
{
	(void)m;
	(void)attr;
}

/* Flush CPU cache for virtual address range */
static __inline void
vm_page_flush_cache(void *va, size_t len)
{
	(void)va;
	(void)len;
}

/* Machine-level TLB shootdown helpers - stubs */
static __inline void
pmap_page_dirty(vm_page_t m)
{
	(void)m;
}

/*
 * NOTE: pmap_clear_modify() and pmap_is_modified() are NOT defined here
 * because DragonFly has real implementations in vm/pmap.h.
 * Those functions are used by the VM system to track page modifications.
 */

/* Sync page for device access */
static __inline void
vm_page_sync_cache(vm_page_t m, boolean_t invalidate)
{
	(void)m;
	(void)invalidate;
}

#endif /* _MACHINE_VM_H_ */
