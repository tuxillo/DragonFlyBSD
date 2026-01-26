# ARM64 DMAP Implementation Plan

## Problem Statement

The arm64 kernel hangs in `vm_page_startup()` when trying to access the
`vm_page_array` via `PHYS_TO_DMAP()`. The current DMAP implementation is
a broken identity map:

```c
/* sys/platform/arm64/include/vmparam.h - CURRENT (BROKEN) */
#define PHYS_TO_DMAP(x)     ((vm_offset_t)(x))
#define DMAP_TO_PHYS(x)     ((vm_paddr_t)(x))
```

This fails because:
- Physical address 0x40000000 returns virtual address 0x40000000
- But VA 0x40000000 is in TTBR0 space (user), not TTBR1 (kernel)
- The kernel runs in TTBR1 (addresses 0xFFFF...)
- Any access to the "DMAP" address causes a fault/hang

## Solution: Proper DMAP Region

Implement a Direct Map (DMAP) region following FreeBSD's approach:

1. Reserve a large virtual address range in kernel space for DMAP
2. Create page table mappings from DMAP VA to physical addresses
3. Update macros to translate PA <-> DMAP VA correctly

## Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| DMAP VA range | 0xffffa00000000000 - 0xffffff0000000000 | Match FreeBSD layout |
| Mapping granularity | 2MB blocks (L2 level) | Simpler code, sufficient for MVP |
| Page table source | Bootstrap allocator | Already working, no new allocation mechanism needed |
| When to initialize | After EFI map parse, before vm_page_startup | Must be ready before PHYS_TO_DMAP() is used |

## Memory Layout

```
ARM64 Virtual Address Space (48-bit):

User space (TTBR0):
  0x0000_0000_0000_0000 - 0x0000_FFFF_FFFF_FFFF

[Hole - non-canonical addresses]

Kernel space (TTBR1):
  0xFFFF_0000_0000_0000 - 0xFFFF_007F_FFFF_FFFF  Kernel VA (512 GiB)
  0xFFFF_A000_0000_0000 - 0xFFFF_FEFF_FFFF_FFFF  DMAP region (95 TiB max)
  
  KERNBASE = 0xFFFF_FF80_0000_0000 (within kernel VA)
```

## DMAP Address Translation

```
Physical Memory:           DMAP Virtual:
  dmap_phys_base -----> DMAP_MIN_ADDRESS (0xffffa00000000000)
       |                      |
  0x40000000            0xffffa00000000000 + (0x40000000 - dmap_phys_base)
  0x40200000            0xffffa00000000000 + (0x40200000 - dmap_phys_base)
       |                      |
  dmap_phys_max -----> dmap_max_addr
```

**Formulas:**
```c
PHYS_TO_DMAP(pa) = (pa - dmap_phys_base) + DMAP_MIN_ADDRESS
DMAP_TO_PHYS(va) = (va - DMAP_MIN_ADDRESS) + dmap_phys_base
```

## Page Table Structure for DMAP

ARM64 4-level page tables with 4KB granule:

```
Level   Entry Size   Coverage per entry   Index bits
L0      8 bytes      512 GiB              VA[47:39]
L1      8 bytes      1 GiB                VA[38:30]
L2      8 bytes      2 MB (block)         VA[29:21]
L3      8 bytes      4 KB (page)          VA[20:12]
```

For DMAP with 2MB blocks:
- L0 table: 1 page (512 entries, covers 256 TiB)
- L1 tables: As needed (each covers 512 GiB)
- L2 tables: As needed (each covers 1 GiB, 512 x 2MB blocks)

**Example for 512MB physical memory (0x40000000 - 0x60000000):**
- Need to map 256 x 2MB blocks
- Requires 1 L2 table (512 entries)
- L1 entry points to L2 table
- L0 entry points to L1 table

## Implementation Steps

### Step 1: Update vmparam.h

Add DMAP constants and fix macros:

```c
/* DMAP region: 95 TiB maximum (matches FreeBSD) */
#define DMAP_MIN_ADDRESS    (0xffffa00000000000UL)
#define DMAP_MAX_ADDRESS    (0xffffff0000000000UL)

#define PMAP_HAS_DMAP       1

/* Helper macros */
#define PHYS_IN_DMAP(pa)    ((pa) >= dmap_phys_base && (pa) < dmap_phys_max)
#define VIRT_IN_DMAP(va)    ((va) >= DMAP_MIN_ADDRESS && (va) < dmap_max_addr)

/* Address translation (requires dmap_phys_base to be initialized) */
#define PHYS_TO_DMAP(pa)    (((pa) - dmap_phys_base) + DMAP_MIN_ADDRESS)
#define DMAP_TO_PHYS(va)    (((va) - DMAP_MIN_ADDRESS) + dmap_phys_base)

#ifndef LOCORE
extern vm_paddr_t dmap_phys_base;
extern vm_paddr_t dmap_phys_max;
extern vm_offset_t dmap_max_addr;
#endif
```

### Step 2: Add Global Variables (pmap.c)

```c
/* DMAP tracking variables */
vm_paddr_t dmap_phys_base;    /* Lowest PA mapped in DMAP (2MB aligned) */
vm_paddr_t dmap_phys_max;     /* Highest PA + 1 */
vm_offset_t dmap_max_addr;    /* Highest VA in DMAP */
```

### Step 3: Implement pmap_bootstrap_dmap()

```c
/*
 * pmap_bootstrap_dmap - Create Direct Map page tables
 *
 * Called early in boot after EFI memory map is parsed.
 * Creates 2MB block mappings for all physical memory in DMAP region.
 *
 * Page table allocation uses arm64_boot_alloc() from bootstrap allocator.
 */
void
pmap_bootstrap_dmap(void)
{
    vm_paddr_t pa, pa_end;
    vm_offset_t va;
    pt_entry_t *l0, *l1, *l2;
    int l0_idx, l1_idx, l2_idx;
    
    /*
     * 1. Determine physical address range from phys_avail[]
     */
    dmap_phys_base = phys_avail[0].phys_beg & ~L2_OFFSET;  /* 2MB align */
    dmap_phys_max = 0;
    for (int i = 0; phys_avail[i].phys_end != 0; i++) {
        if (phys_avail[i].phys_end > dmap_phys_max)
            dmap_phys_max = phys_avail[i].phys_end;
    }
    dmap_phys_max = roundup2(dmap_phys_max, L2_SIZE);
    
    /*
     * 2. Allocate L0 table (reuse existing TTBR1 L0 or allocate new)
     */
    l0 = (pt_entry_t *)arm64_boot_alloc(PAGE_SIZE, PAGE_SIZE);
    bzero_early(l0, PAGE_SIZE);  /* Use physical access */
    
    /*
     * 3. For each 2MB region, create L0->L1->L2 entries
     */
    for (pa = dmap_phys_base; pa < dmap_phys_max; pa += L2_SIZE) {
        va = DMAP_MIN_ADDRESS + (pa - dmap_phys_base);
        
        l0_idx = (va >> L0_SHIFT) & L0_MASK;
        l1_idx = (va >> L1_SHIFT) & L1_MASK;
        l2_idx = (va >> L2_SHIFT) & L2_MASK;
        
        /* Ensure L1 table exists */
        if ((l0[l0_idx] & PTE_V) == 0) {
            l1 = arm64_boot_alloc(PAGE_SIZE, PAGE_SIZE);
            bzero_early(l1, PAGE_SIZE);
            l0[l0_idx] = (vm_paddr_t)l1 | L0_TABLE;
        } else {
            l1 = (pt_entry_t *)(l0[l0_idx] & ~PAGE_MASK);
        }
        
        /* Ensure L2 table exists */
        if ((l1[l1_idx] & PTE_V) == 0) {
            l2 = arm64_boot_alloc(PAGE_SIZE, PAGE_SIZE);
            bzero_early(l2, PAGE_SIZE);
            l1[l1_idx] = (vm_paddr_t)l2 | L1_TABLE;
        } else {
            l2 = (pt_entry_t *)(l1[l1_idx] & ~PAGE_MASK);
        }
        
        /* Create 2MB block mapping */
        l2[l2_idx] = pa | ATTR_AF | ATTR_SH_IS | 
                     ATTR_IDX(VM_MEMATTR_WRITE_BACK) | L2_BLOCK;
    }
    
    /*
     * 4. Install DMAP L0 table in TTBR1
     *    (or merge with existing kernel L0)
     */
    /* ... TBD: either new TTBR1 or merge entries ... */
    
    /*
     * 5. Invalidate TLB
     */
    __asm __volatile(
        "dsb sy\n"
        "tlbi vmalle1\n"
        "dsb sy\n"
        "isb\n"
        ::: "memory"
    );
    
    dmap_max_addr = DMAP_MIN_ADDRESS + (dmap_phys_max - dmap_phys_base);
}
```

### Step 4: Integration Point

Call `pmap_bootstrap_dmap()` in `initarm()` AFTER `phys_avail[]` is populated
but BEFORE any use of `PHYS_TO_DMAP()`:

```c
/* In machdep.c initarm() */

/* ... parse EFI memory map, populate phys_avail[] ... */

/* Set up DMAP before VM init */
pmap_bootstrap_dmap();

/* Now safe to use PHYS_TO_DMAP() */
/* ... continue to vm_mem_init() ... */
```

### Step 5: Handle TTBR1 Merge

The current arm64 boot already has a TTBR1 with kernel mappings. Options:

**Option A: Separate L0 for DMAP**
- Create new L0 table with DMAP entries
- Copy kernel L0 entries to new L0
- Switch TTBR1 to new L0

**Option B: Add DMAP entries to existing L0**
- The DMAP region (0xffffa...) uses different L0 indices than kernel (0xffffff8...)
- Simply add DMAP L0 entries to existing table

Option B is simpler. The kernel L0 entries are at high indices, DMAP at lower.

## Constraints and Edge Cases

1. **Timing**: `dmap_phys_base` must be set before any PHYS_TO_DMAP() call
2. **Alignment**: `dmap_phys_base` must be 2MB aligned for L2 blocks
3. **Gaps**: Physical memory gaps don't need DMAP entries (sparse is OK)
4. **Device memory**: Don't map device regions in DMAP (only conventional RAM)
5. **Bootstrap access**: `bzero()` for new page tables must work before DMAP is ready

## Files to Modify

| File | Changes |
|------|---------|
| `sys/platform/arm64/include/vmparam.h` | DMAP constants, macros, extern decls |
| `sys/platform/arm64/aarch64/pmap.c` | Global vars, `pmap_bootstrap_dmap()` |
| `sys/platform/arm64/aarch64/machdep.c` | Call `pmap_bootstrap_dmap()` in boot |

## Testing

1. Build on VM: `make kernel.debug`
2. Copy to host: `make copy-kernel`
3. Run QEMU test: `make test TEST_TIMEOUT=120`

**Expected output after fix:**
```
vm_mem_init: calling vm_page_startup
vm_page_startup: phys_avail[] ranges (7 total):
  [0] 0x42000000 - 0x44000000 (8192 pages)
  ...
vm_page_startup: page_range=XXXXX
vm_page_startup: entering free queue loop
...
```

## Risks

1. **TTBR1 switch complexity**: Merging DMAP into existing page tables needs care
2. **Early bzero**: Need to zero page tables before DMAP is ready
3. **TLB invalidation**: Must ensure all CPUs see new mappings (single CPU for now)

## References

- FreeBSD `sys/arm64/arm64/pmap.c` - `pmap_bootstrap_dmap()`
- FreeBSD `sys/arm64/include/vmparam.h` - DMAP definitions
- ARM Architecture Reference Manual - Translation table format
