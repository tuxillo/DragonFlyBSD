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

#ifndef _LINUXKPI_SYS_SYSCALLSUBR_H_
#define _LINUXKPI_SYS_SYSCALLSUBR_H_

/*
 * LinuxKPI syscall subsystem compatibility shim.
 *
 * This provides FreeBSD-like syscall helper functions for LinuxKPI.
 * DragonFly has different syscall infrastructure, so we provide stubs
 * and minimal compatibility wrappers.
 *
 * IMPORTANT: Do NOT redefine sy_call_t or struct sysent here - use
 * DragonFly's native definitions from sys/sysent.h.
 */

#include <sys/types.h>
#include <sys/file.h>

#ifdef __DragonFly__
/* DragonFly uses its own sysent infrastructure */
#include <sys/sysent.h>
#include <sys/sysproto.h>
#endif

/* System call flags - FreeBSD compatibility */
#ifndef SYF_CAPENABLED
#define SYF_CAPENABLED  0x0001
#endif
#ifndef SYF_MPSAFE
#define SYF_MPSAFE      0x0002
#endif

/* Common system call entry stubs */
static __inline int
lkpi_kern_open(struct thread *td __unused, const char *path __unused,
    int flags __unused, int mode __unused, int *fd __unused)
{
    return ENOSYS;
}
#define kern_open lkpi_kern_open

static __inline int
lkpi_kern_close(struct thread *td __unused, int fd __unused)
{
    return ENOSYS;
}
#define kern_close lkpi_kern_close

static __inline int
lkpi_kern_ioctl(struct thread *td __unused, int fd __unused,
    unsigned long com __unused, void *data __unused)
{
    return ENOSYS;
}
#define kern_ioctl lkpi_kern_ioctl

static __inline int
lkpi_kern_read(struct thread *td __unused, int fd __unused,
    void *buf __unused, size_t nbyte __unused, off_t offset __unused, 
    size_t *done __unused)
{
    return ENOSYS;
}
#define kern_read lkpi_kern_read

static __inline int
lkpi_kern_write(struct thread *td __unused, int fd __unused,
    const void *buf __unused, size_t nbyte __unused, 
    off_t offset __unused, size_t *done __unused)
{
    return ENOSYS;
}
#define kern_write lkpi_kern_write

/*
 * System call registration stubs.
 * DragonFly doesn't support dynamic syscall registration in the same way.
 */
static __inline int
lkpi_syscall_register(int code __unused, void *new_se __unused,
    void *old_se __unused)
{
    return ENOSYS;
}
#define syscall_register lkpi_syscall_register

static __inline int
lkpi_syscall_deregister(int code __unused, void *old_se __unused)
{
    return ENOSYS;
}
#define syscall_deregister lkpi_syscall_deregister

/* Capability mode helpers - DragonFly doesn't have Capsicum */
#ifndef IN_CAPABILITY_MODE
#define IN_CAPABILITY_MODE(td)  0
#endif

#endif /* _LINUXKPI_SYS_SYSCALLSUBR_H_ */
