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
 *	Physical memory system implementation
 *
 * This module provides segment tracking for physical memory on platforms
 * with non-contiguous memory (VM_PHYSSEG_SPARSE).  It maintains an array
 * of segments and provides PHYS_TO_VM_PAGE() translation that properly
 * handles gaps in physical memory.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_page.h>
#include <vm/vm_phys.h>

#include <machine/vmparam.h>

/*
 * Physical memory segment array.
 * Segments are kept sorted by physical address.
 */
struct vm_phys_seg vm_phys_segs[VM_PHYSSEG_MAX];
int vm_phys_nsegs;

/*
 * Create a physical memory segment.
 * Segments are inserted in sorted order by start address.
 */
static void
vm_phys_create_seg(vm_paddr_t start, vm_paddr_t end)
{
	struct vm_phys_seg *seg;

	if (vm_phys_nsegs >= VM_PHYSSEG_MAX)
		panic("vm_phys_create_seg: too many segments");

	seg = &vm_phys_segs[vm_phys_nsegs++];

	/* Insert in sorted order */
	while (seg > vm_phys_segs && seg[-1].start >= end) {
		*seg = *(seg - 1);
		seg--;
	}

	seg->start = start;
	seg->end = end;
	seg->first_page = NULL;  /* Set later in vm_phys_init() */

	/* Check for overlap with previous segment */
	if (seg != vm_phys_segs && seg[-1].end > start)
		panic("vm_phys_create_seg: overlapping segments "
		    "[%#jx,%#jx) and [%#jx,%#jx)",
		    (uintmax_t)seg[-1].start, (uintmax_t)seg[-1].end,
		    (uintmax_t)start, (uintmax_t)end);
}

/*
 * Add a physical memory segment.
 * Called during early boot to register available physical memory.
 */
void
vm_phys_add_seg(vm_paddr_t start, vm_paddr_t end)
{
	if ((start & PAGE_MASK) != 0)
		panic("vm_phys_add_seg: start %#jx not page aligned",
		    (uintmax_t)start);
	if ((end & PAGE_MASK) != 0)
		panic("vm_phys_add_seg: end %#jx not page aligned",
		    (uintmax_t)end);
	if (start >= end)
		return;

	vm_phys_create_seg(start, end);
}

/*
 * Initialize the physical memory allocator.
 *
 * This function sets up the first_page pointer for each segment.
 * For VM_PHYSSEG_SPARSE, pages are packed contiguously in vm_page_array
 * with no gaps.  For VM_PHYSSEG_DENSE, first_page is computed directly
 * from the segment's starting physical address.
 *
 * Must be called after vm_page_array is initialized.
 */
void
vm_phys_init(void)
{
	struct vm_phys_seg *seg;
	int segind;
#ifdef VM_PHYSSEG_SPARSE
	u_long npages;

	/*
	 * For sparse configurations, segments are packed contiguously
	 * in the vm_page_array with no gaps between segments.
	 */
	npages = 0;
	for (segind = 0; segind < vm_phys_nsegs; segind++) {
		seg = &vm_phys_segs[segind];
		seg->first_page = &vm_page_array[npages];
		npages += atop(seg->end - seg->start);
	}
#else
	/*
	 * For dense configurations (x86_64), compute first_page
	 * directly from the physical address.
	 */
	for (segind = 0; segind < vm_phys_nsegs; segind++) {
		seg = &vm_phys_segs[segind];
		seg->first_page = PHYS_TO_VM_PAGE(seg->start);
	}
#endif
}

/*
 * Find the vm_page corresponding to the given physical address,
 * which must lie within the given physical memory segment.
 */
vm_page_t
vm_phys_seg_paddr_to_vm_page(struct vm_phys_seg *seg, vm_paddr_t pa)
{
	KKASSERT(pa >= seg->start && pa < seg->end);
	return (&seg->first_page[atop(pa - seg->start)]);
}

/*
 * Find the vm_page corresponding to the given physical address.
 * Returns NULL if the address is not within any known segment.
 *
 * This is the core of PHYS_TO_VM_PAGE() for VM_PHYSSEG_SPARSE.
 */

/* Forward declare UART functions for debug output */
#ifdef __aarch64__
extern void vm_uart_puts(const char *s);
extern void vm_uart_puthex(uint64_t val);
extern void vm_uart_putdec(uint64_t val);
static int vm_phys_debug_count = 0;
#endif

vm_page_t
vm_phys_paddr_to_vm_page(vm_paddr_t pa)
{
	struct vm_phys_seg *seg;
#ifdef __aarch64__
	int segind;

	/* Debug: trace first few calls */
	if (vm_phys_debug_count < 5) {
		vm_uart_puts("  p2v: pa=0x");
		vm_uart_puthex(pa);
		vm_uart_puts(" nsegs=");
		vm_uart_putdec(vm_phys_nsegs);
	}

	segind = vm_phys_lookup_segind(pa);

	if (vm_phys_debug_count < 5) {
		vm_uart_puts(" segind=");
		vm_uart_putdec(segind);
	}

	if (segind < vm_phys_nsegs) {
		seg = &vm_phys_segs[segind];
		if (vm_phys_debug_count < 5) {
			vm_uart_puts(" seg.start=0x");
			vm_uart_puthex(seg->start);
			vm_uart_puts(" seg.end=0x");
			vm_uart_puthex(seg->end);
			vm_uart_puts(" first_page=0x");
			vm_uart_puthex((uint64_t)seg->first_page);
		}
		if (pa >= seg->start) {
			vm_page_t result = vm_phys_seg_paddr_to_vm_page(seg, pa);
			if (vm_phys_debug_count < 5) {
				vm_uart_puts(" -> 0x");
				vm_uart_puthex((uint64_t)result);
				vm_uart_puts("\r\n");
				vm_phys_debug_count++;
			}
			return (result);
		}
	}
	if (vm_phys_debug_count < 5) {
		vm_uart_puts(" -> NULL\r\n");
		vm_phys_debug_count++;
	}
	return (NULL);
#else
	seg = vm_phys_paddr_to_seg(pa);
	if (seg != NULL)
		return (vm_phys_seg_paddr_to_vm_page(seg, pa));
	return (NULL);
#endif
}
