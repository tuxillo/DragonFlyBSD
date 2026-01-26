/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * Copyright (c) 2026 The DragonFly Project.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _MACHINE_VMPARAM_H_
#define	_MACHINE_VMPARAM_H_

#ifndef _SYS_TYPES_H_
#include <sys/types.h>
#endif

#define	MAXTSIZ		(32UL*1024*1024*1024)
#ifndef DFLDSIZ
#define	DFLDSIZ		(128UL*1024*1024)
#endif
#ifndef MAXDSIZ
#define	MAXDSIZ		(32UL*1024*1024*1024)
#endif
#ifndef	DFLSSIZ
#define	DFLSSIZ		(8UL*1024*1024)
#endif
#ifndef	MAXSSIZ
#define	MAXSSIZ		(512UL*1024*1024)
#endif
#ifndef	MAXTHRSSIZ
#define	MAXTHRSSIZ	(128UL*1024*1024*1024)
#endif
#ifndef SGROWSIZ
#define	SGROWSIZ	(128UL*1024)
#endif

#define	MAXSLP		20

#define VM_MIN_USER_ADDRESS	((vm_offset_t)0)
#define VM_MAX_USER_ADDRESS	((vm_offset_t)0x0000ffffffffffffUL)

#define USRSTACK		VM_MAX_USER_ADDRESS

#define VM_MAX_ADDRESS		VM_MAX_USER_ADDRESS
#define VM_MIN_ADDRESS		VM_MIN_USER_ADDRESS

#define VM_MIN_KERNEL_ADDRESS	((vm_offset_t)0xffff000000000000UL)
#define VM_MAX_KERNEL_ADDRESS	((vm_offset_t)0xffffffffffffffffUL)

#define KERNBASE		((vm_offset_t)0xffffff8000000000UL)

/*
 * Direct Map (DMAP) region.
 *
 * ARM64 uses a direct map to provide kernel virtual addresses for all
 * physical memory. This allows the kernel to access any physical address
 * without explicit mapping.
 *
 * The DMAP region is 95 TiB (matching FreeBSD's layout):
 *   DMAP_MIN_ADDRESS = 0xffffa00000000000
 *   DMAP_MAX_ADDRESS = 0xffffff0000000000
 *
 * Translation:
 *   PHYS_TO_DMAP(pa) = (pa - dmap_phys_base) + DMAP_MIN_ADDRESS
 *   DMAP_TO_PHYS(va) = (va - DMAP_MIN_ADDRESS) + dmap_phys_base
 */
#define	DMAP_MIN_ADDRESS	((vm_offset_t)0xffffa00000000000UL)
#define	DMAP_MAX_ADDRESS	((vm_offset_t)0xffffff0000000000UL)

#define	PMAP_HAS_DMAP		1

/* Helper macros for bounds checking */
#define	PHYS_IN_DMAP(pa)	((pa) >= dmap_phys_base && (pa) < dmap_phys_max)
#define	VIRT_IN_DMAP(va)	((va) >= DMAP_MIN_ADDRESS && (va) < dmap_max_addr)

/* Address translation macros */
#define	PHYS_TO_DMAP(pa)	(((vm_offset_t)(pa) - dmap_phys_base) + DMAP_MIN_ADDRESS)
#define	DMAP_TO_PHYS(va)	(((vm_paddr_t)(va) - DMAP_MIN_ADDRESS) + dmap_phys_base)

#ifndef LOCORE
extern vm_paddr_t dmap_phys_base;	/* Lowest physical address in DMAP */
extern vm_paddr_t dmap_phys_max;	/* Highest physical address + 1 */
extern vm_offset_t dmap_max_addr;	/* Highest virtual address in DMAP */
#endif

/*
 * Use sparse physical memory segment tracking on arm64.
 * EFI systems often have non-contiguous physical memory regions,
 * and this enables proper PHYS_TO_VM_PAGE() handling for such systems.
 */
#define VM_PHYSSEG_SPARSE

#endif /* _MACHINE_VMPARAM_H_ */
