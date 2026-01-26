/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2002-2006 Rice University
 * Copyright (c) 2007 Alan L. Cox <alc@cs.rice.edu>
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Alan L. Cox,
 * Olivier Crameri, Peter Druschel, Sitaram Iyer, and Juan Navarro.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	__VM_PHYS_H_
#define	__VM_PHYS_H_

/*
 * Physical memory segment descriptor.
 *
 * This structure describes a contiguous region of physical memory.
 * For VM_PHYSSEG_SPARSE configurations (arm64), first_page points to
 * the segment's portion of vm_page_array (packed, no gaps).
 * For VM_PHYSSEG_DENSE configurations (x86_64), first_page can be
 * computed directly from the physical address.
 */

struct vm_page;

struct vm_phys_seg {
	vm_paddr_t	start;		/* Start physical address */
	vm_paddr_t	end;		/* End physical address (exclusive) */
	vm_page_t	first_page;	/* Pointer to first vm_page in segment */
};

/*
 * Maximum number of physical memory segments.
 * EFI systems can have many non-contiguous regions.
 */
#ifndef VM_PHYSSEG_MAX
#define	VM_PHYSSEG_MAX	32
#endif

extern struct vm_phys_seg vm_phys_segs[];
extern int vm_phys_nsegs;

#endif /* !__VM_PHYS_H_ */
