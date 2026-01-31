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

#ifndef _VM_VM_RESERV_H_
#define _VM_VM_RESERV_H_

/* DragonFly VM reservation compatibility for LinuxKPI */

#include <sys/types.h>

/* VM reservation structure */
struct vm_reserv {
    vm_offset_t start;
    vm_size_t size;
    int flags;
};

/* Reservation flags */
#define VM_RESERV_NONE 0x0000
#define VM_RESERV_ALLOCATED 0x0001
#define VM_RESERV_MAPPED 0x0002

/* VM reservation functions - stubs */
static __inline int
vm_reserv_alloc(struct vm_reserv *reserv, vm_size_t size)
{
    return ENOMEM;  /* Stub - no reservations */
}

static __inline void
vm_reserv_free(struct vm_reserv *reserv)
{
    /* Stub */
}

static __inline int
vm_reserv_map(struct vm_reserv *reserv, vm_offset_t start, vm_size_t size)
{
    return ENOMEM;  /* Stub */
}

static __inline void
vm_reserv_unmap(struct vm_reserv *reserv)
{
    /* Stub */
}

static __inline int
vm_reserv_is_mapped(struct vm_reserv *reserv)
{
    return 0;  /* Stub - not mapped */
}

static __inline vm_size_t
vm_reserv_size(struct vm_reserv *reserv)
{
    return reserv ? reserv->size : 0;
}

static __inline vm_offset_t
vm_reserv_start(struct vm_reserv *reserv)
{
    return reserv ? reserv->start : 0;
}

/* Reservation initialization - stub */
static __inline void
vm_reserv_init(struct vm_reserv *reserv)
{
    reserv->start = 0;
    reserv->size = 0;
    reserv->flags = 0;
}

/* Page reservation - stub */
static __inline int
vm_page_reserve(vm_offset_t start, vm_size_t size)
{
    return 0;  /* Stub - always succeeds */
}

static __inline void
vm_page_unreserve(vm_offset_t start, vm_size_t size)
{
    /* Stub */
}

/* Object reservation - stub */
static __inline int
vm_object_reserve_pages(struct vm_object *obj, vm_pindex_t start,
                        vm_page_t *pages, int count)
{
    return 0;  /* Stub */
}

static __inline void
vm_object_unreserve_pages(struct vm_object *obj, vm_pindex_t start, int count)
{
    /* Stub */
}

#endif /* _VM_VM_RESERV_H_ */
