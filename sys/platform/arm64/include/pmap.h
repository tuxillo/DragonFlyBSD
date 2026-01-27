/*
 * Copyright (c) 1991 Regents of the University of California.
 * Copyright (c) 2026 The DragonFly Project.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and William Jolitz of UUNET Technologies Inc.
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

#ifndef _MACHINE_PMAP_H_
#define	_MACHINE_PMAP_H_

#ifndef _SYS_TYPES_H_
#include <sys/types.h>
#endif
#ifndef _MACHINE_PTE_H_
#include <machine/pte.h>
#endif

struct pmap;
struct vm_page;
struct vm_object;

struct md_page {
	long interlock_count;
	long writeable_count_unused;
};

#define MD_PAGE_FREEABLE(m)	\
	(((m)->flags & (PG_MAPPED | PG_WRITEABLE)) == 0)

struct md_object {
	void *dummy_unused;
};

struct pmap_statistics {
	long resident_count;
	long wired_count;
};

typedef struct pmap_statistics *pmap_statistics_t;

typedef char vm_memattr_t;	/* memory attribute type */

/*
 * PTE bit indices for pmap_bits[] array.
 * These provide an abstraction layer between DragonFly's generic VM code
 * and ARM64-specific PTE bit layouts.
 */
#define PG_V_IDX        0   /* Valid */
#define PG_RW_IDX       1   /* Read/Write (ARM64: inverted - 0=RW) */
#define PG_U_IDX        2   /* User accessible */
#define PG_A_IDX        3   /* Accessed */
#define PG_M_IDX        4   /* Modified/Dirty */
#define PG_W_IDX        5   /* Wired (software bit) */
#define PG_MANAGED_IDX  6   /* Managed page (software bit) */
#define PG_N_IDX        7   /* Non-cacheable */
#define PG_NX_IDX       8   /* No Execute */
#define PG_BITS_SIZE    16

#define PROTECTION_CODES_SIZE   8
#define PAT_INDEX_SIZE          8  /* Memory attribute indices */

struct pmap {
	struct pmap_statistics pm_stats;
	pd_entry_t *pm_l0;                  /* L0 page table (kernel VA) */
	vm_paddr_t pm_l0_paddr;             /* L0 page table (physical) */
	uint64_t pmap_bits[PG_BITS_SIZE];
	uint64_t pmap_cache_bits_pte[PAT_INDEX_SIZE];
	uint64_t protection_codes[PROTECTION_CODES_SIZE];
};

typedef struct pmap *pmap_t;

#define pmap_resident_count(pmap)	((pmap)->pm_stats.resident_count)
#define pmap_wired_count(pmap)		((pmap)->pm_stats.wired_count)
#define pmap_resident_tlnw_count(pmap)	\
	((pmap)->pm_stats.resident_count - (pmap)->pm_stats.wired_count)

#ifdef _KERNEL
extern struct pmap *kernel_pmap;
extern char *ptvmmap;

void	pmap_release(struct pmap *pmap);
void	pmap_page_set_memattr(struct vm_page *m, vm_memattr_t ma);
void	pmap_bootstrap(void);

/* Device memory mapping */
void	*pmap_mapdev(vm_paddr_t, vm_size_t);
void	*pmap_mapdev_attr(vm_paddr_t, vm_size_t, int);
void	*pmap_mapdev_uncacheable(vm_paddr_t, vm_size_t);
void	pmap_unmapdev(vm_offset_t, vm_size_t);

/*
 * ARM64 does not emulate AD bits in software; the hardware handles them.
 */
static __inline int
pmap_emulate_ad_bits(pmap_t pmap __unused) {
	return (0);
}
#endif /* _KERNEL */

#endif /* !_MACHINE_PMAP_H_ */
