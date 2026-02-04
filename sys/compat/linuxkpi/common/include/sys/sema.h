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

#ifndef _SYS_SEMA_H_
#define _SYS_SEMA_H_

/* DragonFly semaphore compatibility for LinuxKPI (FreeBSD semaphore) */

#include <sys/types.h>
#include <sys/mutex.h>
#include <sys/mutex2.h>
#include <sys/systm.h>
#include <sys/errno.h>

/* 
 * FreeBSD style semaphore for kernel use.
 * DragonFly uses different synchronization primitives, so this is a compat stub.
 */

struct sema {
    struct mtx sem_mtx;
    int sem_value;
    int sem_waiters;
    const char *sem_name;
};

/* Semaphore type aliases */
typedef struct sema sema_t;

/* Semaphore operations */
static __inline void
sema_init(struct sema *sema, int value, const char *name)
{
    mtx_init(&sema->sem_mtx, name, NULL, MTX_DEF);
    sema->sem_value = value;
    sema->sem_waiters = 0;
    sema->sem_name = name;
}

static __inline void
sema_destroy(struct sema *sema)
{
    mtx_destroy(&sema->sem_mtx);
}

static __inline int
sema_trywait(struct sema *sema)
{
    int result = 0;
    mtx_lock(&sema->sem_mtx);
    if (sema->sem_value > 0) {
        sema->sem_value--;
        result = 1;
    }
    mtx_unlock(&sema->sem_mtx);
    return result;
}

static __inline int
sema_wait(struct sema *sema)
{
    mtx_lock(&sema->sem_mtx);
    while (sema->sem_value == 0) {
        sema->sem_waiters++;
		mtxsleep(&sema->sem_value, &sema->sem_mtx, 0, "semawait", 0);
        sema->sem_waiters--;
    }
    sema->sem_value--;
    mtx_unlock(&sema->sem_mtx);
    return 0;
}

static __inline int
sema_timedwait(struct sema *sema, int timo)
{
    mtx_lock(&sema->sem_mtx);
    while (sema->sem_value == 0) {
        sema->sem_waiters++;
		if (mtxsleep(&sema->sem_value, &sema->sem_mtx, 0, "semawait", timo)) {
            sema->sem_waiters--;
            mtx_unlock(&sema->sem_mtx);
            return EWOULDBLOCK;
        }
        sema->sem_waiters--;
    }
    sema->sem_value--;
    mtx_unlock(&sema->sem_mtx);
    return 0;
}

static __inline void
sema_post(struct sema *sema)
{
    mtx_lock(&sema->sem_mtx);
    sema->sem_value++;
    if (sema->sem_waiters > 0)
        wakeup(&sema->sem_value);
    mtx_unlock(&sema->sem_mtx);
}

static __inline int
sema_value(struct sema *sema)
{
    int value;
    mtx_lock(&sema->sem_mtx);
    value = sema->sem_value;
    mtx_unlock(&sema->sem_mtx);
    return value;
}

/* Kernel semaphore macros */
#define SEMQ_DEF	0
#define SEMQ_NOWAIT	1

static __inline int
semq_wait(struct sema *sema, int flags)
{
    if (flags & SEMQ_NOWAIT)
        return sema_trywait(sema) ? 0 : EWOULDBLOCK;
    return sema_wait(sema);
}

/* Legacy compatibility */
#define sema_getvalue(sema, valp) do { *(valp) = sema_value(sema); } while (0)

#endif /* _SYS_SEMA_H_ */
