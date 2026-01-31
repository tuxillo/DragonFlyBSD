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

#ifndef _VM_VM_RADIX_H_
#define _VM_VM_RADIX_H_

/* DragonFly VM radix tree compatibility for LinuxKPI */

#include <sys/types.h>

/* Radix tree node structure */
struct vm_radix_node {
    struct vm_radix_node *rn_child[2];
    void *rn_value;
    uint64_t rn_index;
};

/* Radix tree structure */
struct vm_radix {
    struct vm_radix_node *rt_root;
    uint64_t rt_count;
};

typedef struct vm_radix vm_radix_t;

/* Radix tree operations - stub implementations */
static __inline void
vm_radix_init(struct vm_radix *rt)
{
    rt->rt_root = NULL;
    rt->rt_count = 0;
}

static __inline int
vm_radix_insert(struct vm_radix *rt, uint64_t index, void *value)
{
    /* Stub - would insert into radix tree */
    return 0;
}

static __inline void *
vm_radix_lookup(struct vm_radix *rt, uint64_t index)
{
    /* Stub - would lookup in radix tree */
    return NULL;
}

static __inline void *
vm_radix_remove(struct vm_radix *rt, uint64_t index)
{
    /* Stub - would remove from radix tree */
    return NULL;
}

static __inline void
vm_radix_free(struct vm_radix *rt)
{
    /* Stub - would free radix tree */
}

static __inline uint64_t
vm_radix_first(struct vm_radix *rt)
{
    /* Stub - would return first index */
    return 0;
}

static __inline uint64_t
vm_radix_next(struct vm_radix *rt, uint64_t index)
{
    /* Stub - would return next index */
    return -1;
}

static __inline uint64_t
vm_radix_firstn(struct vm_radix *rt, uint64_t index)
{
    /* Stub - would return first index >= given index */
    return index;
}

static __inline uint64_t
vm_radix_nextn(struct vm_radix *rt, uint64_t index)
{
    /* Stub - would return next index > given index */
    return index + 1;
}

static __inline int
vm_radix_empty(struct vm_radix *rt)
{
    return rt->rt_count == 0;
}

static __inline uint64_t
vm_radix_entries(struct vm_radix *rt)
{
    return rt->rt_count;
}

/* Radix tree iterator macros */
#define VM_RADIX_FOREACH(idx, rt) \
    for ((idx) = vm_radix_first(rt); (idx) != (uint64_t)-1; \
         (idx) = vm_radix_next(rt, idx))

#define VM_RADIX_FOREACH_FROM(idx, rt) \
    for (; (idx) != (uint64_t)-1; (idx) = vm_radix_next(rt, idx))

#define VM_RADIX_FOREACH_SAFE(idx, rt, tmp) \
    for ((tmp) = vm_radix_first(rt), (idx) = (tmp); \
         (idx) != (uint64_t)-1; \
         (tmp) = vm_radix_next(rt, (tmp)), (idx) = (tmp))

/* Radix tree flags */
#define VM_RADIX_FLAGS_NONE 0x00
#define VM_RADIX_FLAGS_ALLOC 0x01

#endif /* _VM_VM_RADIX_H_ */
