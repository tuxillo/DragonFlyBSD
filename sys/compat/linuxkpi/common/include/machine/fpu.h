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

#ifndef _MACHINE_FPU_H_
#define _MACHINE_FPU_H_

/* DragonFly FPU compatibility for LinuxKPI (FreeBSD FPU context) */

#include <sys/types.h>

/* FPU state structure - x86_64 specific */
struct fpu_kern_ctx {
    uint8_t fpu_state[512];  /* FXSAVE area */
    int valid;
};

/* FPU kernel enter/exit - stubs */
static __inline int
fpu_kern_enter(struct thread *td, struct fpu_kern_ctx *ctx, uint32_t flags)
{
    /* Stub - would save FPU state and disable preemption */
    return 0;
}

static __inline int
fpu_kern_leave(struct thread *td, struct fpu_kern_ctx *ctx)
{
    /* Stub - would restore FPU state and enable preemption */
    return 0;
}

static __inline int
fpu_kern_thread(struct thread *td)
{
    /* Stub - returns 1 if thread uses FPU in kernel */
    return 0;
}

/* FPU flags */
#define FPU_KERN_NORMAL 0x0000
#define FPU_KERN_NOWAIT 0x0001
#define FPU_KERN_NOCTX 0x0002
#define FPU_KERN_KTHR 0x0004

/* FPU context allocation - stubs */
static __inline struct fpu_kern_ctx *
fpu_kern_alloc_ctx(void)
{
    return NULL;  /* Stub */
}

static __inline void
fpu_kern_free_ctx(struct fpu_kern_ctx *ctx)
{
    /* Stub */
}

/* FPU init - stub */
static __inline void
fpuinit(void)
{
    /* Stub */
}

/* Get FPU context - stub */
static __inline struct fpu_kern_ctx *
fpugetctx(struct thread *td)
{
    return NULL;  /* Stub */
}

/* Drop FPU - stub */
static __inline void
fpudrop(void)
{
    /* Stub */
}

#endif /* _MACHINE_FPU_H_ */
