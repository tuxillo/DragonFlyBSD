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

#ifndef _SYS_CAPSICUM_H_
#define _SYS_CAPSICUM_H_

/* DragonFly Capability Mode definitions for LinuxKPI */

#include <sys/types.h>

/*
 * Capsicum is a capability-based security model that provides
 * fine-grained privilege restriction for processes.
 * 
 * This is a minimal stub for LinuxKPI compatibility.
 * DragonFly does not implement Capsicum natively.
 */

/* Capability mode rights (from FreeBSD capsicum) */
#define CAP_NONE        0x0000000000000000ULL
#define CAP_ALL         0xFFFFFFFFFFFFFFFFULL
#define CAP_READ        0x0000000000000001ULL
#define CAP_WRITE       0x0000000000000002ULL
#define CAP_SEEK        0x0000000000000004ULL
#define CAP_MMAP        0x0000000000000008ULL
#define CAP_FSTAT       0x0000000000000010ULL
#define CAP_FCNTL       0x0000000000000020ULL
#define CAP_FEVENT      0x0000000000000040ULL
#define CAP_FSYNC       0x0000000000000080ULL
#define CAP_IOCTL       0x0000000000000100ULL
#define CAP_ACL_CHECK   0x0000000000000200ULL
#define CAP_ACL_DELETE  0x0000000000000400ULL
#define CAP_ACL_GET     0x0000000000000800ULL
#define CAP_ACL_SET     0x0000000000001000ULL
#define CAP_EXTATTR_DELETE  0x0000000000002000ULL
#define CAP_EXTATTR_GET     0x0000000000004000ULL
#define CAP_EXTATTR_LIST    0x0000000000008000ULL
#define CAP_EXTATTR_SET     0x0000000000010000ULL
#define CAP_FCHDIR      0x0000000000020000ULL
#define CAP_FCHFLAGS    0x0000000000040000ULL
#define CAP_FCHMOD      0x0000000000080000ULL
#define CAP_FCHOWN      0x0000000000100000ULL
#define CAP_FLOCK       0x0000000000200000ULL
#define CAP_FPATHCONF   0x0000000000400000ULL
#define CAP_FSCK        0x0000000000800000ULL
#define CAP_FSTATFS     0x0000000001000000ULL
#define CAP_FUTIMES     0x0000000002000000ULL
#define CAP_GETPEERNAME 0x0000000004000000ULL
#define CAP_GETSOCKNAME 0x0000000008000000ULL
#define CAP_GETSOCKOPT  0x0000000010000000ULL
#define CAP_LISTEN      0x0000000020000000ULL
#define CAP_MAC_GET     0x0000000040000000ULL
#define CAP_MAC_SET     0x0000000080000000ULL
#define CAP_MKDIR       0x0000000100000000ULL
#define CAP_MKFIFO      0x0000000200000000ULL
#define CAP_MMAP_X      0x0000000400000000ULL
#define CAP_RMDIR       0x0000000800000000ULL
#define CAP_SETSOCKOPT  0x0000001000000000ULL
#define CAP_SHUTDOWN    0x0000002000000000ULL
#define CAP_UNLINK      0x0000004000000000ULL
#define CAP_LINKAT      0x0000008000000000ULL
#define CAP_LOOKUP      0x0000010000000000ULL
#define CAP_PREAD       0x0000020000000000ULL
#define CAP_PWRITE      0x0000040000000000ULL
#define CAP_READDIR     0x0000080000000000ULL
#define CAP_RENAMEAT    0x0000100000000000ULL
#define CAP_SYMLINKAT   0x0000200000000000ULL
#define CAP_CONNECT     0x0000400000000000ULL
#define CAP_ACCEPT      0x0000800000000000ULL
#define CAP_BIND        0x0001000000000000ULL
#define CAP_SEND        0x0002000000000000ULL
#define CAP_RECV        0x0004000000000000ULL
#define CAP_POLL_EVENT  0x0008000000000000ULL
#define CAP_TTY_PRIV    0x0010000000000000ULL
#define CAP_TTY_VNLAST  0x0020000000000000ULL
#define CAP_TTY_VNEOF   0x0040000000000000ULL
#define CAP_TTY_SETALT  0x0080000000000000ULL
#define CAP_TTY_SETA    0x0100000000000000ULL
#define CAP_TTY_DRAIN   0x0200000000000000ULL
#define CAP_TTY_FLUSH   0x0400000000000000ULL
#define CAP_TTY_HELPER  0x0800000000000000ULL
#define CAP_SOCK_CLIENT 0x1000000000000000ULL
#define CAP_SOCK_SERVER 0x2000000000000000ULL

/* Capability mode file descriptor limit */
#define CAP_FCNTL_GETFL 0x01
#define CAP_FCNTL_SETFL 0x02
#define CAP_FCNTL_GETOWN 0x04
#define CAP_FCNTL_SETOWN 0x08

/* Capability mode ioctls */
#ifndef IOCBASECMD
#define IOCBASECMD(cmd)     ((cmd) & ~(0xFF << 16))
#endif
#ifndef IOCGROUP
#define IOCGROUP(cmd)       (((cmd) >> 8) & 0xFF)
#endif

/* Capability rights structure (stub) */
struct cap_rights {
    uint64_t cr_rights[2];  /* Capability rights */
};
typedef struct cap_rights cap_rights_t;

/* Capability mode status for process */
struct cap_mode {
    int cm_enabled;     /* Capability mode enabled */
    uint64_t cm_rights; /* Default rights */
};

/*
 * Capability rights operations (stubs)
 */

/* Initialize capability rights to none */
static __inline void
cap_rights_init(struct cap_rights *rights, ...)
{
    if (rights) {
        rights->cr_rights[0] = CAP_NONE;
        rights->cr_rights[1] = CAP_NONE;
    }
}

/* Set capability rights to all */
static __inline void
cap_rights_set(struct cap_rights *rights, ...)
{
    if (rights) {
        rights->cr_rights[0] = CAP_ALL;
        rights->cr_rights[1] = CAP_ALL;
    }
}

/* Clear capability rights */
static __inline void
cap_rights_clear(struct cap_rights *rights, ...)
{
    if (rights) {
        rights->cr_rights[0] = CAP_NONE;
        rights->cr_rights[1] = CAP_NONE;
    }
}

/* Check if capability rights are set */
static __inline int
cap_rights_is_set(const struct cap_rights *rights, ...)
{
    (void)rights;
    return 0;
}

/* Check if capability rights contain all specified rights */
static __inline int
cap_rights_contains(const struct cap_rights *big, const struct cap_rights *little)
{
    (void)big; (void)little;
    return 1;  /* Always allow for stubs */
}

/* Get capability rights for a file descriptor (stub) */
static __inline int
cap_rights_get(int fd, struct cap_rights *rights)
{
    (void)fd;
    if (rights) {
        rights->cr_rights[0] = CAP_ALL;
        rights->cr_rights[1] = CAP_ALL;
    }
    return 0;
}

/* Set capability rights for a file descriptor (stub) */
static __inline int
cap_rights_limit(int fd, const struct cap_rights *rights)
{
    (void)fd; (void)rights;
    return 0;  /* Success - no actual restriction in stub */
}

/* Enter capability mode (stub) */
static __inline int
cap_enter(void)
{
    /* No-op in DragonFly stub */
    return 0;
}

/* Check if process is in capability mode (stub) */
static __inline int
cap_getmode(unsigned int *mode)
{
    if (mode)
        *mode = 0;  /* Not in capability mode */
    return 0;
}

/* Capability ioctls limit (stub) */
static __inline int
cap_ioctls_limit(int fd, const unsigned long *cmds, size_t ncmds)
{
    (void)fd; (void)cmds; (void)ncmds;
    return 0;
}

/* Get permitted ioctls (stub) */
static __inline ssize_t
cap_ioctls_get(int fd, unsigned long *cmds, size_t maxcmds)
{
    (void)fd; (void)cmds; (void)maxcmds;
    return 0;
}

/* Capability fcntls limit (stub) */
static __inline int
cap_fcntls_limit(int fd, uint32_t fcntls)
{
    (void)fd; (void)fcntls;
    return 0;
}

/* Get permitted fcntls (stub) */
static __inline int
cap_fcntls_get(int fd, uint32_t *fcntls)
{
    if (fcntls)
        *fcntls = CAP_FCNTL_GETFL | CAP_FCNTL_SETFL | 
                  CAP_FCNTL_GETOWN | CAP_FCNTL_SETOWN;
    return 0;
}

/* Convenience macro for rights initialization */
#define CAP_RIGHTS_INIT(rights, ...) cap_rights_init(rights, __VA_ARGS__, 0)

/* Check if in capability mode */
#define IN_CAPABILITY_MODE() (0)

/* Capability-aware functions */
#define cap_open(path, flags, ...)      open(path, flags, ##__VA_ARGS__)
#define cap_fopen(path, mode)           fopen(path, mode)
#define cap_mkdir(path, mode)           mkdir(path, mode)
#define cap_mkfifo(path, mode)          mkfifo(path, mode)
#define cap_mknod(path, mode, dev)      mknod(path, mode, dev)
#define cap_rename(from, to)            rename(from, to)
#define cap_rmdir(path)                 rmdir(path)
#define cap_unlink(path)                unlink(path)
#define cap_symlink(target, linkpath)   symlink(target, linkpath)
#define cap_link(from, to)              link(from, to)
#define cap_chdir(path)                 chdir(path)
#define cap_execve(path, argv, envp)    execve(path, argv, envp)

#endif /* _SYS_CAPSICUM_H_ */
