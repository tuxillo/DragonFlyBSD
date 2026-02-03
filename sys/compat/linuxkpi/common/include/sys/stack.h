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

#ifndef _SYS_STACK_H_
#define _SYS_STACK_H_

/* DragonFly stack compatibility for LinuxKPI (FreeBSD stack tracing) */

#include <sys/types.h>
#include <sys/systm.h>

/* Stack structure - simplified for DragonFly */
/* NOTE: Renamed from 'stack_t' to 'lkpi_stack_t' to avoid conflict
 * with DragonFly's signal stack type in sys/_ucontext.h */
struct lkpi_stack {
    void *frames[64];  /* Up to 64 stack frames */
    int depth;
};

typedef struct lkpi_stack lkpi_stack_t;

/* Stack operations - stub implementations */
static __inline void
stack_zero(lkpi_stack_t *st)
{
    st->depth = 0;
}

static __inline void
stack_save(lkpi_stack_t *st)
{
    /* Stub - DragonFly has different stack tracing mechanism */
    st->depth = 0;
}

static __inline void
stack_save_td(lkpi_stack_t *st, void *td)
{
    stack_zero(st);
}

static __inline int
stack_depth(lkpi_stack_t *st)
{
    return st->depth;
}

static __inline void *
stack_get(lkpi_stack_t *st, int i)
{
    if (i >= 0 && i < st->depth)
        return st->frames[i];
    return NULL;
}

static __inline void
stack_copy(lkpi_stack_t *src, lkpi_stack_t *dst)
{
    *dst = *src;
}

static __inline int
stack_cmp(lkpi_stack_t *s1, lkpi_stack_t *s2)
{
    if (s1->depth != s2->depth)
        return s1->depth - s2->depth;
    /* Compare frames */
    for (int i = 0; i < s1->depth; i++) {
        if (s1->frames[i] != s2->frames[i])
            return (s1->frames[i] > s2->frames[i]) ? 1 : -1;
    }
    return 0;
}

static __inline void
stack_print(lkpi_stack_t *st)
{
    print_backtrace(-1);
}

static __inline void
stack_print_short(lkpi_stack_t *st)
{
    stack_print(st);
}

static __inline void
stack_print_ddb(lkpi_stack_t *st)
{
    stack_print(st);
}

/* Stack save with skip - stub */
static __inline void
stack_save_td_running(lkpi_stack_t *st, void *td)
{
    stack_save_td(st, td);
}

static __inline void
stack_save_td_missing(lkpi_stack_t *st, void *td)
{
    stack_save_td(st, td);
}

static __inline void
stack_save_args(lkpi_stack_t *st, void *td)
{
    stack_save_td(st, td);
}

/* Stack hash - stub */
static __inline uint32_t
stack_hash(lkpi_stack_t *st)
{
    return 0;
}

/* Stack put - stub */
static __inline void
stack_put(lkpi_stack_t *st, int idx, void *pc)
{
    if (idx >= 0 && idx < 64)
        st->frames[idx] = pc;
}

#endif /* _SYS_STACK_H_ */
