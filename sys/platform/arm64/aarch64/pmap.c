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
 * 16 pages = 16 * 512 entries = 8192 PTEs = 32MB of mappable KVA.
 * This matches FreeBSD's approach for early boot before kmalloc works.
 */
#define NKERN_L3_PAGES	16
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
 * Allocate a new L3 page table from the pre-allocated pool.
 * This is used during early boot before kmalloc is available.
 */
static pt_entry_t *
pmap_alloc_l3(pmap_t pmap __unused, pd_entry_t *l2, vm_offset_t va)
{
	pt_entry_t *l3;
	vm_paddr_t l3_pa;

	if (kern_l3_next >= NKERN_L3_PAGES)
		panic("pmap_alloc_l3: out of pre-allocated L3 pages");

	l3 = kern_l3_pages[kern_l3_next++];
	bzero(l3, PAGE_SIZE);

	/*
	 * Get physical address of the L3 table. The kern_l3_pages array
	 * is in kernel BSS, so use pmap_kextract() which handles both
	 * DMAP and kernel address translation.
	 */
	l3_pa = pmap_kextract((vm_offset_t)l3);

	/* Install L2 entry pointing to new L3 table */
	*l2 = l3_pa | L2_TABLE;
	__asm __volatile("dsb ishst" ::: "memory");

	return (&l3[pmap_l3_index(va)]);
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
	}
}

/*
 * Remove a mapping.
 */
void
pmap_remove(pmap_t pmap __unused, vm_offset_t sva __unused,
    vm_offset_t eva __unused)
{
}

/*
 * Protect mappings.
 */
void
pmap_protect(pmap_t pmap __unused, vm_offset_t sva __unused,
    vm_offset_t eva __unused, vm_prot_t prot __unused)
{
}

/*
 * Unwire a page.
 */
vm_page_t
pmap_unwire(pmap_t pmap __unused, vm_offset_t *vap __unused)
{
	return (NULL);
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
 */
void
pmap_kremove(vm_offset_t va __unused)
{
}

/*
 * Remove a kernel mapping quickly.
 */
void
pmap_kremove_quick(vm_offset_t va __unused)
{
}

/*
 * Enter multiple pages at once.
 */
void
pmap_qenter(vm_offset_t va __unused, struct vm_page **m __unused,
    int count __unused)
{
}

/*
 * Enter multiple pages without invalidation.
 */
void
pmap_qenter_noinval(vm_offset_t va __unused, struct vm_page **m __unused,
    int count __unused)
{
}

/*
 * Remove multiple pages.
 */
void
pmap_qremove(vm_offset_t va __unused, int count __unused)
{
}

/*
 * Remove multiple pages without invalidation.
 */
void
pmap_qremove_noinval(vm_offset_t va __unused, int count __unused)
{
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
 */
vm_page_t
pmap_kvtom(vm_offset_t va __unused)
{
	return (NULL);
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
 */
void
pmap_bootstrap(void)
{
	/* Set up kernel_pmap to use TTBR1 page tables */
	kernel_pmap->pm_l0 = ttbr1_l0;
	kernel_pmap->pm_l0_paddr = DMAP_TO_PHYS((vm_offset_t)ttbr1_l0);

	/* Copy default bit mappings */
	bcopy(pmap_bits_default, kernel_pmap->pmap_bits,
	      sizeof(pmap_bits_default));
	bcopy(pmap_cache_bits_default, kernel_pmap->pmap_cache_bits_pte,
	      sizeof(pmap_cache_bits_default));

	/* Initialize protection codes */
	pmap_init_protection_codes();
}
