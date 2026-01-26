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

/*
 *	Physical memory system definitions
 */

#ifndef	_VM_PHYS_H_
#define	_VM_PHYS_H_

#ifdef _KERNEL

#include <vm/_vm_phys.h>

/*
 * The following functions are only to be used by the virtual memory system.
 */
void vm_phys_add_seg(vm_paddr_t start, vm_paddr_t end);
void vm_phys_init(void);
/* vm_phys_paddr_to_vm_page is declared in vm_page.h for PHYS_TO_VM_PAGE macro */
vm_page_t vm_phys_seg_paddr_to_vm_page(struct vm_phys_seg *seg, vm_paddr_t pa);

/*
 * Find the segment index for the first segment at or after the given
 * physical address.  Uses binary search.
 */
static __inline int
vm_phys_lookup_segind(vm_paddr_t pa)
{
	u_int hi, lo, mid;

	lo = 0;
	hi = vm_phys_nsegs;
	while (lo != hi) {
		/*
		 * for i in [0, lo), segs[i].end <= pa
		 * for i in [hi, nsegs), segs[i].end > pa
		 */
		mid = lo + (hi - lo) / 2;
		if (vm_phys_segs[mid].end <= pa)
			lo = mid + 1;
		else
			hi = mid;
	}
	return (lo);
}

/*
 * Find the segment corresponding to the given physical address.
 * Returns NULL if the address is not within any segment.
 */
static __inline struct vm_phys_seg *
vm_phys_paddr_to_seg(vm_paddr_t pa)
{
	struct vm_phys_seg *seg;
	int segind;

	segind = vm_phys_lookup_segind(pa);
	if (segind < vm_phys_nsegs) {
		seg = &vm_phys_segs[segind];
		if (pa >= seg->start)
			return (seg);
	}
	return (NULL);
}

#endif	/* _KERNEL */
#endif	/* !_VM_PHYS_H_ */
