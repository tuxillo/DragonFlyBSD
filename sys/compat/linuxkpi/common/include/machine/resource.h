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

#ifndef _LINUXKPI_MACHINE_RESOURCE_H_
#define _LINUXKPI_MACHINE_RESOURCE_H_

/*
 * LinuxKPI machine/resource.h compatibility for DragonFly BSD.
 *
 * FreeBSD has this header to define architecture-specific resource types.
 * DragonFly defines everything we need in sys/bus.h and sys/rman.h.
 * Just include those and provide any missing definitions.
 */

#ifdef __DragonFly__

#include <sys/bus.h>
#include <sys/rman.h>

/*
 * FreeBSD-specific resource types that DragonFly might not define.
 * Check if they exist before defining.
 */

/* SYS_RES_* types should be defined in sys/bus.h */
#ifndef SYS_RES_IRQ
#define SYS_RES_IRQ	1
#endif
#ifndef SYS_RES_DRQ
#define SYS_RES_DRQ	2
#endif
#ifndef SYS_RES_MEMORY
#define SYS_RES_MEMORY	3
#endif
#ifndef SYS_RES_IOPORT
#define SYS_RES_IOPORT	4
#endif

/* RF_* flags should be defined in sys/rman.h */
#ifndef RF_ALLOCATED
#define RF_ALLOCATED	0x0001
#endif
#ifndef RF_ACTIVE
#define RF_ACTIVE	0x0002
#endif
#ifndef RF_SHAREABLE
#define RF_SHAREABLE	0x0004
#endif
#ifndef RF_PREFETCHABLE
#define RF_PREFETCHABLE	0x0040
#endif
#ifndef RF_OPTIONAL
#define RF_OPTIONAL	0x0080
#endif
#ifndef RF_UNMAPPED
#define RF_UNMAPPED	0x0100	/* FreeBSD 13+ */
#endif

#else /* FreeBSD */

/* On FreeBSD, include the native header */
#include_next <machine/resource.h>

#endif /* __DragonFly__ */

#endif /* _LINUXKPI_MACHINE_RESOURCE_H_ */
