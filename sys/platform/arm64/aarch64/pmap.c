/*-
 * Copyright (c) 2026 The DragonFly Project.  All rights reserved.
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
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * ARM64 pmap - stub implementation for compile-only MVP.
 * Real implementation will follow FreeBSD's arm64 pmap.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/msgbuf.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_pager.h>
#include <machine/pmap.h>
#include <machine/pte.h>
#include <machine/md_var.h>
#include <machine/pcb.h>

/*
 * Kernel pmap and VM globals
 */
static struct pmap kernel_pmap_store;
struct pmap *kernel_pmap = &kernel_pmap_store;

vm_phystable_t phys_avail[16];
vm_phystable_t dump_avail[16];
long physmem;

/*
 * Direct Map (DMAP) global variables.
 * These track the physical address range mapped into the DMAP region.
 */
vm_paddr_t dmap_phys_base;	/* Lowest physical address in DMAP (2MB aligned) */
vm_paddr_t dmap_phys_max;	/* Highest physical address + 1 */
vm_offset_t dmap_max_addr;	/* Highest virtual address in DMAP */

vm_offset_t virtual_start;
vm_offset_t virtual_end;
vm_offset_t virtual2_start;
vm_offset_t virtual2_end;
vm_offset_t kernel_vm_end;

vm_offset_t KvaStart;
vm_offset_t KvaEnd;
vm_offset_t KvaSize;

/*
 * Page tables from locore.s
 */
extern pd_entry_t ttbr0_l0[];
extern pd_entry_t ttbr0_l1[];
extern pd_entry_t ttbr1_l0[];
extern pd_entry_t ttbr1_l1[];
extern pd_entry_t ttbr1_l2[];

/*
 * Pre-allocated L3 pages for early kernel mappings.
 * 64 pages = 64 * 512 entries = 32768 PTEs = 128MB of mappable KVA.
 * Increased from 16 to handle device driver KVA allocations during boot.
 * This matches FreeBSD's approach for early boot before kmalloc works.
 */
#define NKERN_L3_PAGES	64
static pt_entry_t kern_l3_pages[NKERN_L3_PAGES][Ln_ENTRIES] __aligned(PAGE_SIZE);
static int kern_l3_next = 0;

/*
 * Default PTE bit mappings for ARM64.
 * These map DragonFly's logical bit indices to ARM64 PTE bits.
 */
static uint64_t pmap_bits_default[PG_BITS_SIZE] = {
	[PG_V_IDX]       = ATTR_DESCR_VALID,
	[PG_RW_IDX]      = 0,                    /* 0 = RW on ARM64 */
	[PG_U_IDX]       = ATTR_S1_AP(ATTR_S1_AP_USER),
	[PG_A_IDX]       = ATTR_AF,
	[PG_M_IDX]       = ATTR_DBM,
	[PG_W_IDX]       = (1UL << 55),          /* Software: wired */
	[PG_MANAGED_IDX] = (1UL << 56),          /* Software: managed */
	[PG_NX_IDX]      = ATTR_S1_XN,
};

/*
 * Memory attribute index to PTE bits mapping.
 */
static uint64_t pmap_cache_bits_default[PAT_INDEX_SIZE] = {
	[0] = ATTR_S1_IDX(MAIR_IDX_DEVICE_nGnRnE),
	[1] = ATTR_S1_IDX(MAIR_IDX_UNCACHEABLE),
	[2] = ATTR_S1_IDX(MAIR_IDX_WRITE_BACK),
	[3] = ATTR_S1_IDX(MAIR_IDX_WRITE_THROUGH),
	[4] = ATTR_S1_IDX(MAIR_IDX_DEVICE_nGnRE),
};

/*
 * Protection codes: VM_PROT_* -> PTE bits.
 * Initialized by pmap_init_protection_codes().
 */
static uint64_t protection_codes_default[PROTECTION_CODES_SIZE];

/*
 * Page table walking helper functions.
 * These navigate the ARM64 4-level page table structure.
 */
static __inline pd_entry_t *
pmap_l0(pmap_t pmap, vm_offset_t va)
{
	return (&pmap->pm_l0[pmap_l0_index(va)]);
}

static __inline pd_entry_t *
pmap_l0_to_l1(pd_entry_t *l0, vm_offset_t va)
{
	pd_entry_t l0e = *l0;
	pd_entry_t *l1 = (pd_entry_t *)PHYS_TO_DMAP(l0e & ATTR_ADDR);
	return (&l1[pmap_l1_index(va)]);
}

static __inline pd_entry_t *
pmap_l1_to_l2(pd_entry_t *l1, vm_offset_t va)
{
	pd_entry_t l1e = *l1;
	pd_entry_t *l2 = (pd_entry_t *)PHYS_TO_DMAP(l1e & ATTR_ADDR);
	return (&l2[pmap_l2_index(va)]);
}

static __inline pt_entry_t *
pmap_l2_to_l3(pd_entry_t *l2, vm_offset_t va)
{
	pd_entry_t l2e = *l2;
	pt_entry_t *l3 = (pt_entry_t *)PHYS_TO_DMAP(l2e & ATTR_ADDR);

	return (&l3[pmap_l3_index(va)]);
}

/*
 * Get a pointer to the PTE for a given virtual address.
 * Returns NULL if any level of page table is not present.
 * This is used for operations that need to check if a mapping exists
 * without allocating new page tables.
 */
static pt_entry_t *
pmap_pte(pmap_t pmap, vm_offset_t va)
{
	pd_entry_t *l0, *l1, *l2;

	if (pmap == NULL || pmap->pm_l0 == NULL)
		return (NULL);

	l0 = pmap_l0(pmap, va);
	if ((*l0 & ATTR_DESCR_MASK) != L0_TABLE)
		return (NULL);

	l1 = pmap_l0_to_l1(l0, va);
	if ((*l1 & ATTR_DESCR_MASK) != L1_TABLE)
		return (NULL);

	l2 = pmap_l1_to_l2(l1, va);
	if ((*l2 & ATTR_DESCR_MASK) != L2_TABLE)
		return (NULL);

	return pmap_l2_to_l3(l2, va);
}

/*
 * Allocate a new L3 page table from the pre-allocated pool.
 * This is used during early boot before kmalloc is available.
 *
 * IMPORTANT: Returns a DMAP address for the L3 table to ensure all
 * accesses to the L3 page go through the same virtual alias.
 * This prevents cache coherency issues between BSS and DMAP mappings.
 */
static pt_entry_t *
pmap_alloc_l3(pmap_t pmap __unused, pd_entry_t *l2, vm_offset_t va)
{
	pt_entry_t *l3_bss;
	pt_entry_t *l3_dmap;
	vm_paddr_t l3_pa;

	if (kern_l3_next >= NKERN_L3_PAGES)
		panic("pmap_alloc_l3: out of pre-allocated L3 pages");

	/* Get BSS address for zeroing */
	l3_bss = kern_l3_pages[kern_l3_next++];
	bzero(l3_bss, PAGE_SIZE);

	/*
	 * Get physical address of the L3 table. The kern_l3_pages array
	 * is in kernel BSS, so use pmap_kextract() which handles both
	 * DMAP and kernel address translation.
	 */
	l3_pa = pmap_kextract((vm_offset_t)l3_bss);

	/*
	 * Convert to DMAP address for returning. This ensures all subsequent
	 * accesses to this L3 table (via pmap_l2_to_l3) use the same virtual
	 * alias, preventing cache coherency issues.
	 */
	l3_dmap = (pt_entry_t *)PHYS_TO_DMAP(l3_pa);

	/*
	 * Ensure the zero operation is visible through DMAP before we
	 * install the L2 entry and return the DMAP pointer.
	 */
	__asm __volatile("dsb ish" ::: "memory");

	/* Install L2 entry pointing to new L3 table */
	*l2 = l3_pa | L2_TABLE;
	__asm __volatile("dsb ishst" ::: "memory");

	return (&l3_dmap[pmap_l3_index(va)]);
}

/*
 * Get a pointer to the PTE for a given virtual address.
 * Allocates L3 tables as needed from the pre-allocated pool.
 */
static pt_entry_t *
pmap_vtopte(pmap_t pmap, vm_offset_t va)
{
	pd_entry_t *l0, *l1, *l2;

	l0 = pmap_l0(pmap, va);
	if ((*l0 & ATTR_DESCR_MASK) != L0_TABLE)
		panic("pmap_vtopte: no L0 entry for va 0x%lx", va);

	l1 = pmap_l0_to_l1(l0, va);
	if ((*l1 & ATTR_DESCR_MASK) != L1_TABLE)
		panic("pmap_vtopte: no L1 entry for va 0x%lx", va);

	l2 = pmap_l1_to_l2(l1, va);
	if ((*l2 & ATTR_DESCR_MASK) != L2_TABLE) {
		/* Need to allocate L3 table */
		return pmap_alloc_l3(pmap, l2, va);
	}

	return pmap_l2_to_l3(l2, va);
}

/*
 * Initialize the pmap module.
 */
void
pmap_init(void)
{
}

/*
 * Secondary pmap initialization.
 */
void
pmap_init2(void)
{
}

/*
 * Initialize a pmap for a new process.
 */
void
pmap_pinit0(struct pmap *pmap)
{
	pmap->pm_stats.resident_count = 0;
	pmap->pm_stats.wired_count = 0;
}

/*
 * Initialize a pmap structure.
 */
void
pmap_pinit(struct pmap *pmap)
{
	pmap->pm_stats.resident_count = 0;
	pmap->pm_stats.wired_count = 0;
}

/*
 * Secondary pmap initialization after first process is created.
 */
void
pmap_pinit2(struct pmap *pmap)
{
}

/*
 * Release any resources held by the pmap structure.
 */
void
pmap_puninit(struct pmap *pmap __unused)
{
}

/*
 * Release reference to the pmap.
 */
void
pmap_release(struct pmap *pmap __unused)
{
}

/*
 * Reference the pmap.
 */
void
pmap_reference(struct pmap *pmap __unused)
{
}

/*
 * Create a pv_entry for the mapping.
 */
void
pmap_page_init(struct vm_page *m __unused)
{
}

/*
 * Initialize the thread pmap.
 *
 * Set up td_pcb at the top of the kernel stack, properly aligned.
 * This is called from lwkt_init_thread() after td_kstack is set.
 */
void
pmap_init_thread(struct thread *td)
{
	/* Place PCB at top of kernel stack, 16-byte aligned */
	td->td_pcb = (struct pcb *)(td->td_kstack + td->td_kstack_size) - 1;
	td->td_pcb = (struct pcb *)((intptr_t)td->td_pcb & ~(intptr_t)0xF);
	td->td_sp = (char *)td->td_pcb;
}

/*
 * Initialize the proc pmap.
 */
void
pmap_init_proc(struct proc *p __unused)
{
}

/*
 * Enter a mapping.
 *
 * Maps a physical page into the given pmap's address space with the
 * specified protection. Updates vm_page flags and pmap statistics.
 */
void
pmap_enter(pmap_t pmap, vm_offset_t va, struct vm_page *m, vm_prot_t prot,
    boolean_t wired, struct vm_map_entry *entry __unused)
{
	pt_entry_t *ptep;
	pt_entry_t newpte, origpte;
	vm_paddr_t pa;
	int flags, nflags;

	if (pmap == NULL)
		return;

	va = trunc_page(va);
	pa = VM_PAGE_TO_PHYS(m);

	/* Get PTE pointer */
	ptep = pmap_vtopte(pmap, va);
	origpte = *ptep;

	/* Build new PTE */
	newpte = pa | L3_PAGE | ATTR_AF | ATTR_SH(ATTR_SH_IS);
	newpte |= ATTR_S1_IDX(MAIR_IDX_WRITE_BACK);

	/* Protection bits */
	if (!(prot & VM_PROT_WRITE))
		newpte |= ATTR_S1_AP(ATTR_S1_AP_RO);
	if (!(prot & VM_PROT_EXECUTE))
		newpte |= ATTR_S1_XN;
	if (va < VM_MAX_USER_ADDRESS)
		newpte |= ATTR_S1_AP(ATTR_S1_AP_USER) | ATTR_S1_PXN;

	/* Software bits */
	if (wired)
		newpte |= pmap->pmap_bits[PG_W_IDX];
	if ((m->flags & PG_FICTITIOUS) == 0)
		newpte |= pmap->pmap_bits[PG_MANAGED_IDX];

	/* Update vm_page flags */
	flags = m->flags;
	for (;;) {
		nflags = PG_MAPPED;
		if (prot & VM_PROT_WRITE)
			nflags |= PG_WRITEABLE;
		if (flags & PG_MAPPED)
			nflags |= PG_MAPPEDMULTI;
		if (flags == (flags | nflags))
			break;
		if (atomic_fcmpset_int(&m->flags, &flags, flags | nflags))
			break;
	}

	/* Store the PTE */
	*ptep = newpte;

	/* TLB invalidate */
	__asm __volatile(
		"dsb ishst\n"
		"tlbi vaae1is, %0\n"
		"dsb ish\n"
		"isb"
		: : "r"(va >> 12)
	);

	/* Update statistics */
	if ((origpte & ATTR_DESCR_VALID) == 0) {
		pmap->pm_stats.resident_count++;
	}
	if (wired && (origpte == 0 || !(origpte & pmap->pmap_bits[PG_W_IDX]))) {
		pmap->pm_stats.wired_count++;
		/*
		 * Wire the page in the VM system.  This increments m->wire_count
		 * which will be decremented by vm_page_unwire() when pmap_unwire()
		 * returns this page during unwiring.
		 */
		if ((m->flags & PG_FICTITIOUS) == 0)
			vm_page_wire(m);
	}
}

/*
 * Handle a removed PTE for a managed page.
 * Clear PG_MAPPED/PG_WRITEABLE flags and unwire if necessary.
 */
static __inline void
pmap_removed_pte(pmap_t pmap, vm_page_t m, pt_entry_t pte)
{
	int flags;
	int nflags;

	/*
	 * Clear PG_MAPPED and PG_WRITEABLE unless the page is mapped
	 * in multiple locations (PG_MAPPEDMULTI).
	 */
	flags = m->flags;
	cpu_ccfence();
	while ((flags & PG_MAPPEDMULTI) == 0) {
		nflags = flags & ~(PG_MAPPED | PG_WRITEABLE);
		if (atomic_fcmpset_int(&m->flags, &flags, nflags))
			break;
	}

	/*
	 * If the page was wired, unwire it now.
	 */
	if (pte & pmap->pmap_bits[PG_W_IDX])
		vm_page_unwire(m, -1);
}

/*
 * Remove a range of mappings from the pmap.
 *
 * Iterates through the virtual address range, clearing PTEs and
 * invalidating TLB entries. Updates pmap statistics.
 */
void
pmap_remove(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	pt_entry_t *ptep;
	pt_entry_t oldpte;
	vm_offset_t va;
	vm_page_t m;
	vm_paddr_t pa;

	if (pmap == NULL)
		return;

	for (va = sva; va < eva; va += PAGE_SIZE) {
		ptep = pmap_pte(pmap, va);
		if (ptep == NULL)
			continue;

		oldpte = *ptep;
		if ((oldpte & ATTR_DESCR_VALID) == 0)
			continue;

		/* Clear the PTE */
		*ptep = 0;

		/* Update statistics */
		pmap->pm_stats.resident_count--;
		if (oldpte & pmap->pmap_bits[PG_W_IDX])
			pmap->pm_stats.wired_count--;

		/* Handle managed page removal */
		if (oldpte & pmap->pmap_bits[PG_MANAGED_IDX]) {
			pa = oldpte & ATTR_ADDR;
			m = PHYS_TO_VM_PAGE(pa);
			if (m != NULL)
				pmap_removed_pte(pmap, m, oldpte);
		}
	}

	/* TLB invalidate the range */
	if (sva < eva) {
		__asm __volatile("dsb ishst" ::: "memory");
		for (va = sva; va < eva; va += PAGE_SIZE) {
			__asm __volatile("tlbi vaae1is, %0" : : "r"(va >> 12));
		}
		__asm __volatile("dsb ish; isb" ::: "memory");
	}
}

/*
 * Protect mappings in a range.
 *
 * Update the access permission bits for all valid PTEs in the range.
 */
void
pmap_protect(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, vm_prot_t prot)
{
	pt_entry_t *ptep;
	pt_entry_t oldpte, newpte;
	vm_offset_t va;

	if (pmap == NULL)
		return;

	/* If removing all permissions, use pmap_remove instead */
	if ((prot & VM_PROT_READ) == 0) {
		pmap_remove(pmap, sva, eva);
		return;
	}

	for (va = sva; va < eva; va += PAGE_SIZE) {
		ptep = pmap_pte(pmap, va);
		if (ptep == NULL)
			continue;

		oldpte = *ptep;
		if ((oldpte & ATTR_DESCR_VALID) == 0)
			continue;

		/* Build new PTE with updated protection */
		newpte = oldpte;

		/* Clear and reset AP bits */
		newpte &= ~(ATTR_S1_AP_MASK);
		if (!(prot & VM_PROT_WRITE))
			newpte |= ATTR_S1_AP(ATTR_S1_AP_RO);

		/* Handle execute permission */
		if (!(prot & VM_PROT_EXECUTE))
			newpte |= ATTR_S1_XN;
		else
			newpte &= ~ATTR_S1_XN;

		if (newpte != oldpte)
			*ptep = newpte;
	}

	/* TLB invalidate the range */
	if (sva < eva) {
		__asm __volatile("dsb ishst" ::: "memory");
		for (va = sva; va < eva; va += PAGE_SIZE) {
			__asm __volatile("tlbi vaae1is, %0" : : "r"(va >> 12));
		}
		__asm __volatile("dsb ish; isb" ::: "memory");
	}
}

/*
 * Unwire a page.
 *
 * Called in a loop by vm_fault_unwire() with va < end condition.
 * MUST advance *vap to prevent infinite loops.
 *
 * For a full implementation, this would:
 * - Look up the PTE for *vap
 * - Clear the wired bit if present
 * - Return the vm_page if found
 * - Decrement pmap wired_count stats
 *
 * For now, we implement a minimal version that just advances
 * the address and optionally clears wired bits if found.
 */
vm_page_t
pmap_unwire(pmap_t pmap, vm_offset_t *vap)
{
	pt_entry_t *ptep;
	pt_entry_t pte;
	vm_page_t m = NULL;
	vm_offset_t va = *vap;

	/* Always advance the address to prevent infinite loops */
	*vap = va + PAGE_SIZE;

	if (pmap == NULL)
		return (NULL);

	/* Try to find the PTE */
	ptep = pmap_pte(pmap, va);
	if (ptep == NULL)
		return (NULL);

	pte = *ptep;
	if ((pte & ATTR_DESCR_VALID) == 0)
		return (NULL);

	/* Check if page is wired */
	if (pte & pmap->pmap_bits[PG_W_IDX]) {
		/* Clear wired bit */
		*ptep = pte & ~pmap->pmap_bits[PG_W_IDX];
		pmap->pm_stats.wired_count--;

		/* TLB invalidate this page */
		__asm __volatile(
			"dsb ishst\n"
			"tlbi vaae1is, %0\n"
			"dsb ish\n"
			"isb"
			: : "r"(va >> 12)
		);

		/* Return the vm_page if managed */
		if (pte & pmap->pmap_bits[PG_MANAGED_IDX]) {
			vm_paddr_t pa = pte & ATTR_ADDR;
			m = PHYS_TO_VM_PAGE(pa);
		}
	}

	return (m);
}

/*
 * Copy mappings from one pmap to another.
 */
void
pmap_copy(pmap_t dst_pmap __unused, pmap_t src_pmap __unused,
    vm_offset_t dst_addr __unused, vm_size_t len __unused,
    vm_offset_t src_addr __unused)
{
}

/*
 * Extract the physical address for a virtual address.
 */
vm_paddr_t
pmap_extract(pmap_t pmap __unused, vm_offset_t va __unused, void **handlep)
{
	if (handlep)
		*handlep = NULL;
	return (0);
}

/*
 * Complete extraction.
 */
void
pmap_extract_done(void *handle __unused)
{
}

/*
 * Physical address of the kernel load address.
 * Set by initarm() based on where UEFI loaded us.
 */
vm_paddr_t kern_phys_base = 0x40000000UL;	/* Default QEMU load address */

/*
 * Extract kernel virtual address to physical.
 *
 * For DMAP addresses, use the DMAP_TO_PHYS macro.
 * For kernel text/data/bss addresses (KERNBASE region), use the
 * AT S1E1R instruction to query the MMU for the translation.
 */
vm_paddr_t
pmap_kextract(vm_offset_t va)
{
	uint64_t par;

	/* Check if this is a DMAP address */
	if (va >= DMAP_MIN_ADDRESS && va < DMAP_MAX_ADDRESS)
		return (DMAP_TO_PHYS(va));

	/*
	 * For kernel addresses, use address translation instruction.
	 * AT S1E1R performs a stage 1 EL1 read translation and stores
	 * the result in PAR_EL1.
	 */
	__asm __volatile(
		"at s1e1r, %1\n"
		"isb\n"
		"mrs %0, par_el1"
		: "=r"(par) : "r"(va)
	);

	/* Check for translation failure (bit 0 set) */
	if (par & 1) {
		/*
		 * Translation failed. For kernel BSS during early boot,
		 * fall back to simple KERNBASE-relative calculation.
		 */
		if (va >= KERNBASE)
			return (kern_phys_base + (va - KERNBASE));
		return (0);
	}

	/* Extract physical address from PAR_EL1 */
	return ((par & 0x0000fffffffff000UL) | (va & PAGE_MASK));
}

/*
 * Enter a kernel mapping.
 *
 * Maps a physical address into the kernel address space with
 * read/write/execute permissions and write-back caching.
 */
void
pmap_kenter(vm_offset_t va, vm_paddr_t pa)
{
	pt_entry_t *ptep;
	pt_entry_t npte;

	npte = pa | L3_PAGE | ATTR_AF | ATTR_SH(ATTR_SH_IS) |
	       ATTR_S1_IDX(MAIR_IDX_WRITE_BACK);

	ptep = pmap_vtopte(kernel_pmap, va);
	*ptep = npte;

	/* TLB invalidate */
	__asm __volatile(
		"dsb ishst\n"
		"tlbi vaae1is, %0\n"
		"dsb ish\n"
		"isb"
		: : "r"(va >> 12)
	);
}

/*
 * Enter a kernel mapping without invalidation.
 */
int
pmap_kenter_noinval(vm_offset_t va, vm_paddr_t pa)
{
	pt_entry_t *ptep;
	pt_entry_t npte;

	npte = pa | L3_PAGE | ATTR_AF | ATTR_SH(ATTR_SH_IS) |
	       ATTR_S1_IDX(MAIR_IDX_WRITE_BACK);

	ptep = pmap_vtopte(kernel_pmap, va);
	*ptep = npte;

	/* No TLB invalidate - caller will handle batched invalidation */
	__asm __volatile("dsb ishst" ::: "memory");

	return (0);
}

/*
 * Enter a kernel mapping quickly.
 *
 * Same as pmap_kenter() for now. In the future this could skip
 * locking for single-CPU early boot.
 */
int
pmap_kenter_quick(vm_offset_t va, vm_paddr_t pa)
{
	pmap_kenter(va, pa);
	return (0);
}

/*
 * Remove a kernel mapping.
 *
 * Clear the PTE and invalidate the TLB for this kernel VA.
 */
void
pmap_kremove(vm_offset_t va)
{
	pt_entry_t *ptep;

	ptep = pmap_pte(kernel_pmap, va);
	if (ptep != NULL && (*ptep & ATTR_DESCR_VALID)) {
		*ptep = 0;

		/* TLB invalidate */
		__asm __volatile(
			"dsb ishst\n"
			"tlbi vaae1is, %0\n"
			"dsb ish\n"
			"isb"
			: : "r"(va >> 12)
		);
	}
}

/*
 * Remove a kernel mapping quickly.
 *
 * Same as pmap_kremove() - for ARM64 we always do full TLB invalidation.
 */
void
pmap_kremove_quick(vm_offset_t va)
{
	pmap_kremove(va);
}

/*
 * Enter multiple pages at once.
 *
 * Map an array of vm_pages into consecutive kernel virtual addresses.
 */
void
pmap_qenter(vm_offset_t va, struct vm_page **m, int count)
{
	int i;

	for (i = 0; i < count; i++) {
		pmap_kenter(va, VM_PAGE_TO_PHYS(m[i]));
		va += PAGE_SIZE;
	}
}

/*
 * Enter multiple pages without invalidation.
 *
 * For ARM64 we still do TLB invalidation for safety.
 */
void
pmap_qenter_noinval(vm_offset_t va, struct vm_page **m, int count)
{
	pmap_qenter(va, m, count);
}

/*
 * Remove multiple pages.
 *
 * Unmap consecutive kernel virtual addresses.
 */
void
pmap_qremove(vm_offset_t va, int count)
{
	int i;

	for (i = 0; i < count; i++) {
		pmap_kremove(va);
		va += PAGE_SIZE;
	}
}

/*
 * Remove multiple pages without invalidation.
 *
 * For ARM64 we still do TLB invalidation for safety.
 */
void
pmap_qremove_noinval(vm_offset_t va, int count)
{
	pmap_qremove(va, count);
}

/*
 * Zero a page.
 *
 * With identity DMAP, we can directly access the physical address.
 */
void
pmap_zero_page(vm_paddr_t pa)
{
	bzero((void *)PHYS_TO_DMAP(pa), PAGE_SIZE);
}

/*
 * Zero a page area.
 */
void
pmap_zero_page_area(vm_paddr_t pa, int off, int size)
{
	bzero((char *)PHYS_TO_DMAP(pa) + off, size);
}

/*
 * Copy a page.
 */
void
pmap_copy_page(vm_paddr_t src, vm_paddr_t dst)
{
	bcopy((void *)PHYS_TO_DMAP(src), (void *)PHYS_TO_DMAP(dst), PAGE_SIZE);
}

/*
 * Page is modified?
 */
boolean_t
pmap_is_modified(struct vm_page *m __unused)
{
	return (FALSE);
}

/*
 * Clear modify bit.
 */
void
pmap_clear_modify(struct vm_page *m __unused)
{
}

/*
 * Clear reference bit.
 */
void
pmap_clear_reference(struct vm_page *m __unused)
{
}

/*
 * Test reference.
 */
int
pmap_ts_referenced(struct vm_page *m __unused)
{
	return (0);
}

/*
 * Remove all mappings from a page.
 */
void
pmap_page_protect(struct vm_page *m __unused, vm_prot_t prot __unused)
{
}

/*
 * Synchronize mapped state.
 */
int
pmap_mapped_sync(struct vm_page *m __unused)
{
	return (0);
}

/*
 * Collect garbage.
 */
void
pmap_collect(void)
{
}

/*
 * Remove all pages from a pmap.
 */
void
pmap_remove_pages(pmap_t pmap __unused, vm_offset_t sva __unused,
    vm_offset_t eva __unused)
{
}

/*
 * Remove specific mapping.
 */
void
pmap_remove_specific(pmap_t pmap __unused, struct vm_page *m __unused)
{
}

/*
 * Grow kernel VA.
 */
void
pmap_growkernel(vm_offset_t kstart __unused, vm_offset_t kend __unused)
{
}

/*
 * Map physical memory.
 *
 * For arm64, we use the direct map (DMAP) approach like FreeBSD.
 * Physical addresses are directly accessible via PHYS_TO_DMAP().
 * This works because we maintain identity mapping in TTBR0 during
 * early boot, and later will set up a proper DMAP region.
 *
 * The virt pointer is advanced by the mapped size for compatibility
 * with callers that expect sequential virtual allocations.
 */
vm_offset_t
pmap_map(vm_offset_t *virt, vm_paddr_t start, vm_paddr_t end, int prot __unused)
{
	vm_size_t size = end - start;

	/* Advance the virtual address pointer (for caller tracking) */
	if (virt != NULL)
		*virt += size;

	/* Return direct-mapped address */
	return (PHYS_TO_DMAP(start));
}

/*
 * Object initialization.
 */
void
pmap_object_init(struct vm_object *object __unused)
{
}

/*
 * Object free.
 */
void
pmap_object_free(struct vm_object *object __unused)
{
}

/*
 * Initialize page table for object.
 */
void
pmap_object_init_pt(pmap_t pmap __unused, struct vm_map_entry *entry __unused,
    vm_offset_t addr __unused, vm_offset_t size __unused,
    int pagelimit __unused)
{
}

/*
 * Prefault check.
 */
int
pmap_prefault_ok(pmap_t pmap __unused, vm_offset_t addr __unused)
{
	return (0);
}

/*
 * Mincore - determine if page is in memory.
 */
int
pmap_mincore(pmap_t pmap __unused, vm_offset_t addr __unused)
{
	return (0);
}

/*
 * Maybe threaded check.
 */
void
pmap_maybethreaded(pmap_t pmap __unused)
{
}

/*
 * Invalidate range.
 */
void
pmap_invalidate_range(pmap_t pmap __unused, vm_offset_t sva __unused,
    vm_offset_t eva __unused)
{
}

/*
 * Get physical address.
 */
vm_offset_t
pmap_phys_address(vm_pindex_t ppn __unused)
{
	return (0);
}

/*
 * Provide address hint.
 */
vm_offset_t
pmap_addr_hint(struct vm_object *obj __unused, vm_offset_t addr,
    vm_size_t size __unused)
{
	return (addr);
}

/*
 * Replace VM in LWP.
 */
void
pmap_replacevm(struct proc *p __unused, struct vmspace *newvm __unused,
    int flags __unused)
{
}

/*
 * Set LWP vmspace.
 */
void
pmap_setlwpvm(struct lwp *lp __unused, struct vmspace *newvm __unused)
{
}

/*
 * Set page memory attribute.
 */
void
pmap_page_set_memattr(struct vm_page *m __unused, vm_memattr_t ma __unused)
{
}

/*
 * Kernel virtual to machine page.
 *
 * Given a kernel virtual address, return the vm_page_t for the
 * physical page backing that address. This is used by the slab
 * allocator to track page usage (ku_pagecnt).
 *
 * Returns NULL if the address is not mapped or not backed by a
 * managed page.
 */
vm_page_t
pmap_kvtom(vm_offset_t va)
{
	pt_entry_t *ptep;
	pt_entry_t pte;
	vm_paddr_t pa;

	/* Get PTE for this VA */
	ptep = pmap_pte(kernel_pmap, va);
	if (ptep == NULL)
		return (NULL);

	pte = *ptep;
	if ((pte & ATTR_DESCR_VALID) == 0)
		return (NULL);

	/* Extract physical address from PTE */
	pa = pte & ATTR_ADDR;

	/* Convert to vm_page_t */
	return (PHYS_TO_VM_PAGE(pa));
}

/*
 * Scan pages.
 */
void
pmap_pgscan(struct pmap_pgscan_info *info __unused)
{
}

/*
 * Fault page quick.
 */
struct vm_page *
pmap_fault_page_quick(pmap_t pmap __unused, vm_offset_t va __unused,
    vm_prot_t prot __unused, int *busyp)
{
	if (busyp)
		*busyp = 0;
	return (NULL);
}

/*
 * Check if address is in globaldata space.
 */
int
is_globaldata_space(vm_offset_t addr __unused, vm_offset_t size __unused)
{
	return (0);
}

/*
 * Initialize protection codes.
 *
 * This pre-computes PTE bits for each possible VM_PROT_* combination.
 */
static void
pmap_init_protection_codes(void)
{
	uint64_t *kp = protection_codes_default;
	int prot;

	for (prot = 0; prot < PROTECTION_CODES_SIZE; prot++) {
		*kp = 0;

		/* ARM64: AP[2]=1 means read-only, AP[2]=0 means read-write */
		if (!(prot & VM_PROT_WRITE))
			*kp |= ATTR_S1_AP(ATTR_S1_AP_RO);

		/* XN bit for non-executable */
		if (!(prot & VM_PROT_EXECUTE))
			*kp |= ATTR_S1_XN;

		kp++;
	}

	bcopy(protection_codes_default, kernel_pmap->protection_codes,
	      sizeof(protection_codes_default));
}

/*
 * Bootstrap the pmap module.
 *
 * This is called early in arm64_init() to set up the kernel pmap
 * before any VM operations that need pmap_enter()/pmap_kenter().
 *
 * Note: By this point, arm64_pmap_bootstrap() has already set up new
 * page tables and arm64_ttbr1_switch() has switched TTBR1 to use them.
 * We read TTBR1_EL1 to get the actual L0 table address.
 */
void
pmap_bootstrap(void)
{
	uint64_t ttbr1;

	/* Read current TTBR1_EL1 to get the active L0 table physical address */
	__asm __volatile("mrs %0, ttbr1_el1" : "=r"(ttbr1));

	/*
	 * Set up kernel_pmap to use the current TTBR1 page tables.
	 * The L0 table is at the physical address in TTBR1_EL1.
	 * We access it via DMAP.
	 */
	kernel_pmap->pm_l0_paddr = ttbr1 & ~0xfffUL;  /* Mask off ASID bits */
	kernel_pmap->pm_l0 = (pd_entry_t *)PHYS_TO_DMAP(kernel_pmap->pm_l0_paddr);

	/* Copy default bit mappings */
	bcopy(pmap_bits_default, kernel_pmap->pmap_bits,
	      sizeof(pmap_bits_default));
	bcopy(pmap_cache_bits_default, kernel_pmap->pmap_cache_bits_pte,
	      sizeof(pmap_cache_bits_default));

	/* Initialize protection codes */
	pmap_init_protection_codes();
}

/*
 * Map device memory into the kernel address space.
 *
 * For ARM64 with DMAP covering all physical memory, we simply return
 * the DMAP address for the physical address. This provides a valid
 * kernel virtual address that can access the device memory.
 *
 * Note: This simple implementation assumes DMAP covers the device's
 * physical address range. For devices outside DMAP (unlikely on
 * typical systems), a more complex implementation would need to
 * allocate KVA and create explicit mappings with device memory
 * attributes.
 */
void *
pmap_mapdev(vm_paddr_t pa, vm_size_t size __unused)
{
	return ((void *)PHYS_TO_DMAP(pa));
}

/*
 * Map device memory with specific memory attributes.
 *
 * For now, same as pmap_mapdev() - we rely on DMAP.
 * A full implementation would set up page tables with the
 * requested memory attributes.
 */
void *
pmap_mapdev_attr(vm_paddr_t pa, vm_size_t size, int mode __unused)
{
	return (pmap_mapdev(pa, size));
}

/*
 * Map device memory as uncacheable.
 */
void *
pmap_mapdev_uncacheable(vm_paddr_t pa, vm_size_t size)
{
	return (pmap_mapdev(pa, size));
}

/*
 * Unmap device memory.
 *
 * Since pmap_mapdev() returns DMAP addresses, there's nothing
 * to actually unmap - the DMAP is permanent.
 */
void
pmap_unmapdev(vm_offset_t va __unused, vm_size_t size __unused)
{
	/* Nothing to do for DMAP-based mappings */
}
