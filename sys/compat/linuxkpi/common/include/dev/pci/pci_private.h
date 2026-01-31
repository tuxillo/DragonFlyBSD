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

#ifndef _LINUXKPI_DEV_PCI_PCI_PRIVATE_H_
#define _LINUXKPI_DEV_PCI_PCI_PRIVATE_H_

/*
 * LinuxKPI wrapper for PCI private internals.
 * This header redirects to DragonFly's native PCI headers.
 */

/* Include DragonFly's native PCI headers */
#include <bus/pci/pcivar.h>
#include <bus/pci/pcireg.h>

/*
 * FreeBSD-specific PCI internal definitions that DragonFly doesn't have.
 * These are stubbed out for LinuxKPI compatibility.
 */

/* rman_res_t doesn't exist in DragonFly - use u_long instead */
#ifndef rman_res_t
typedef u_long rman_res_t;
#endif

/* PCI power states (if not defined) */
#ifndef PCI_POWERSTATE_D0
#define PCI_POWERSTATE_D0       0
#define PCI_POWERSTATE_D1       1
#define PCI_POWERSTATE_D2       2
#define PCI_POWERSTATE_D3       3
#define PCI_POWERSTATE_UNKNOWN  255
#endif

/* PCI config space size (if not defined) */
#ifndef PCI_CFG_SPACE_SIZE
#define PCI_CFG_SPACE_SIZE      256
#define PCI_CFG_SPACE_EXP_SIZE  4096
#endif

#endif /* _LINUXKPI_DEV_PCI_PCI_PRIVATE_H_ */
