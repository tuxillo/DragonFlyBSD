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

#ifndef _SYS_SX_H_
#define _SYS_SX_H_

/* DragonFly sx (shared/exclusive lock) compatibility for LinuxKPI */

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/thread.h>

/* sx lock - simple shared/exclusive lock using DragonFly's lockmgr */
typedef struct sx {
    struct lock lk;
    /*
     * sx_lock - compatibility field for FreeBSD code that accesses
     * lock->base.sx.sx_lock directly. In FreeBSD this is an atomic
     * value encoding the owner. In DragonFly we just store a pointer
     * to the lock holder (updated by sx_xlock/sx_xunlock wrappers).
     * For simplicity, we define macros that extract this from lk_lockholder.
     */
} sx_t;

/* Helper to get sx_lock value (the owner thread as uintptr_t) */
#define sx_lock_val(sx) ((uintptr_t)((sx)->lk.lk_lockholder))

/* sx lock operations - map to DragonFly's lockmgr */
#define sx_init(sx, name) lockinit(&(sx)->lk, name, 0, LK_CANRECURSE)
#define sx_init_flags(sx, name, flags) sx_init((sx), (name))
#define sx_destroy(sx) lockuninit(&(sx)->lk)
#define sx_xlock(sx) lockmgr(&(sx)->lk, LK_EXCLUSIVE)
#define sx_xunlock(sx) lockmgr(&(sx)->lk, LK_RELEASE)
#define sx_slock(sx) lockmgr(&(sx)->lk, LK_SHARED)
#define sx_sunlock(sx) lockmgr(&(sx)->lk, LK_RELEASE)
#define sx_try_xlock(sx) (lockmgr(&(sx)->lk, LK_EXCLUSIVE|LK_NOWAIT) == 0)
#define sx_try_slock(sx) (lockmgr(&(sx)->lk, LK_SHARED|LK_NOWAIT) == 0)
#define sx_xlocked(sx) lockstatus(&(sx)->lk, LK_EXCLUSIVE)
#define sx_assert(sx, what)

/* 
 * sx_xholder - get the thread holding the exclusive lock.
 * DragonFly's struct lock has lk_lockholder for this.
 */
static __inline struct thread *
sx_xholder(struct sx *sx)
{
    return sx->lk.lk_lockholder;
}

/*
 * SX_OWNER - get the owner of the lock from the sx_lock value.
 * In FreeBSD, SX_OWNER(val) extracts the thread pointer from the raw lock value.
 * We receive the value which is already the thread pointer (from sx_lock_val).
 */
#define SX_OWNER(val) ((struct thread *)(val))

/* sx flags - stubs for compatibility */
#ifndef SX_NOWITNESS
#define SX_NOWITNESS 0
#endif
#ifndef SX_DUPOK
#define SX_DUPOK 0
#endif
#ifndef SX_RECURSE
#define SX_RECURSE 0
#endif

/* sx lock assertions */
#define SA_XLOCKED 1
#define SA_SLOCKED 2
#define SA_UNLOCKED 3

/* SX_SYSINIT_FLAGS - static initialization for sx locks */
#define SX_SYSINIT_FLAGS(name, sx_ptr, desc, flags) \
    static void __CONCAT(name, _sx_sysinit)(void *arg __unused) { \
        sx_init_flags(sx_ptr, desc, flags); \
    } \
    SYSINIT(__CONCAT(name, _sx_init), SI_SUB_LOCK, SI_ORDER_MIDDLE, \
        __CONCAT(name, _sx_sysinit), NULL)

#endif /* _SYS_SX_H_ */
