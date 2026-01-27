# ARM64 VM Page Startup Hang Investigation

## Status: ALL ISSUES RESOLVED ✅

The arm64 kernel now successfully boots to the `mountroot>` prompt. All issues tracked in this document have been resolved.

## Issue Summary

The arm64 kernel hung during `vm_page_startup()` when adding physical pages to free queues. This document tracks the investigation and resolution of multiple issues encountered during ARM64 kernel bringup.

## Issues Resolved

### Issue 1: Memory Barrier Functions (Commit a7edc9d061)

**Root Cause:** Broken memory barrier functions in `sys/cpu/aarch64/include/cpufunc.h`.

The `cpu_sfence()` and `cpu_lfence()` functions were implemented as compiler-only fences:

```c
static __inline void cpu_sfence(void) { __asm __volatile("" ::: "memory"); }
static __inline void cpu_lfence(void) { __asm __volatile("" ::: "memory"); }
```

ARM64 has **weak memory ordering** - the CPU can reorder stores and loads. These compiler-only fences prevent compiler reordering but NOT hardware reordering.

**Fix:**
```c
static __inline void cpu_sfence(void) { __asm __volatile("dmb ishst" ::: "memory"); }
static __inline void cpu_lfence(void) { __asm __volatile("dmb ishld" ::: "memory"); }
```

### Issue 2: ALIST_RECORDS Buffer Overflow (Commit 2dcd46ccf9)

**Root Cause:** Buffer overflow in `sys/sys/alist.h` ALIST_RECORDS macros.

The `alst_radix_init()` function accesses array indices up to `skip` value when writing terminator entries, but `rootblks` only counted *used* records.

**Before (incorrect):**
```c
#define ALIST_RECORDS_65536    2193
#define ALIST_RECORDS_1048576  34961
```

**After (correct):**
```c
#define ALIST_RECORDS_65536    4097
#define ALIST_RECORDS_1048576  65537
```

The overflow was corrupting `vm_page_queues[135]` with the value `0xffffffff00000000`, which later caused the PC to jump to invalid addresses (literal pool data at `0x40100a00`).

### Issue 3: x18 Register Clobbering (Commit d3d859db2f)

**Status:** RESOLVED

**Root Cause:** The GCC cross-compiler uses x18 as a general-purpose register, but ARM64 reserves x18 as a platform register. DragonFly uses x18 to hold the per-CPU globaldata pointer (`mycpu`).

**Symptoms:**
- Kernel crashes in `kmem_init()` with Data Abort
- ESR=0x96000035, FAR=0x20003544 (x18 + offset, but x18 is corrupted)
- Expected x18 value: `0x40ccb000` (address of `arm64_gd0`)
- Actual x18 value: `0x20000000` (corrupted)

**Evidence from objdump:**
```asm
401021c0:   mov w18, #0xa           // WRONG - compiler writing to x18!
401021c4:   csel w18, w18, w1, ne   // WRONG
401021d4:   csel w18, w1, w18, eq   // WRONG
```

**Fix:** Added `-ffixed-x18` and `-mgeneral-regs-only` to kernel CFLAGS in `sys/platform/arm64/conf/kern.mk`, following FreeBSD's pattern (`.freebsd.orig/sys/conf/kern.mk:146`).

### Issue 4: pmap_enter() Stub (RESOLVED - Commit ad57b7bd56)

**Status:** RESOLVED

**Root Cause:** The `pmap_enter()` function in `sys/platform/arm64/aarch64/pmap.c` was an empty stub. When `kmem_slab_alloc()` allocates memory and calls `pmap_enter()` to map it into the kernel virtual address space, nothing happened.

**Symptoms:**
- Kernel crashes during `SI_BOOT1_ALLOCATOR` (0x1400000) - slab allocator initialization
- Data Abort in `memset()` when trying to zero newly allocated memory
- ESR=0x96000046, FAR=0xffffff8008dd3000 (unmapped kernel VA)
- ELR=0x4040d370 (memset inner loop)

**Fix:** Implemented `pmap_enter()`, `pmap_kenter()`, and supporting page table walking functions in commit ad57b7bd56. The implementation includes:
- Page table walking helpers (pmap_l0_to_l1, pmap_l1_to_l2, pmap_l2_to_l3)
- Pre-allocated L3 page pool for early boot
- Proper PTE attribute handling for ARM64
- TLB invalidation sequences

### Issue 5: Missing cpu_startup() (Commit 1b68068698)

**Status:** RESOLVED ✅

**Root Cause:** ARM64 was missing the `cpu_startup()` function that initializes `pager_map`, `buffer_map`, and `clean_map` VM submaps.

**Symptoms:**
- Kernel panics at SI_BOOT2_MACHDEP (0x1d80000)
- Error: "Not enough pager_map VM space for physical buffers"
- `vm_pager_bufferinit()` fails because `pager_map` is NULL

**Fix:** Implemented `cpu_startup()` for ARM64 following x86_64 pattern. Registered with SYSINIT at SI_BOOT2_START_CPU. See MVP Part 7 in `doc/arm64-efi-loader-mvp-part1.md`.

### Issue 6: Missing tsc_frequency/tsc_present

**Status:** RESOLVED ✅

**Root Cause:** `kern_clock.c` and `kern_nrandom.c` reference x86-specific TSC variables.

**Fix:** Added ARM64 equivalents in `generic_timer.c`:
- `tsc_frequency` = ARM64 generic timer counter frequency (CNTFRQ_EL0)
- `tsc_present` = 1 (ARM64 generic timer is always present)

## Investigation Timeline

### Session 1: Initial Debugging

1. Added UART debug output to `vm_page_startup()` to trace progress
2. Discovered hang occurs during the page addition loop
3. Narrowed down to `vm_add_new_page()` function

### Session 2: Binary Search for Hang Location

1. Added granular debug output at each step in `vm_add_new_page()`
2. **Key observation**: Pages completed successfully when debug output was enabled, but hung when debug was disabled
3. **Conclusion**: Debug output (UART writes) was acting as a timing barrier, masking a memory ordering issue
4. Fixed memory barriers in `cpu_sfence()` and `cpu_lfence()`

### Session 3: QMP Debugging and ALIST Fix

1. Used QMP to query CPU state when hung
2. Found PC stuck at `0x40100a00` - literal pool data, not code!
3. Traced corruption back to ALIST buffer overflow
4. Fixed ALIST_RECORDS constants
5. Kernel now progresses past `vm_page_startup()` into `kmem_init()`

### Session 4: x18 Register Investigation and Fix

1. Kernel crashes in `kmem_init()` with DABT exception
2. Fault address `0x20003544` = corrupted x18 + offset
3. Used objdump to find compiler writes to x18
4. Identified missing `-ffixed-x18` flag
5. **Fixed** by adding `-ffixed-x18` and `-mgeneral-regs-only` to kern.mk (commit d3d859db2f)

### Session 5: pmap_enter() Investigation

1. After x18 fix, kernel reaches further - crashes in `SI_BOOT1_ALLOCATOR` (0x1400000)
2. ESR=0x96000046 (DABT translation fault), FAR=0xffffff8008dd3000 (unmapped kernel VA)
3. Crash is in `memset()` - trying to zero memory that was never mapped
4. Root cause: `pmap_enter()` is an empty stub
5. Created full implementation plan as MVP Part 6 in `doc/arm64-efi-loader-mvp-part1.md`

## Test Results

| Date | Commit | Result | Notes |
|------|--------|--------|-------|
| 2026-01-26 | a7edc9d061 | PARTIAL | Memory barrier fix - system reaches vm_page_startup |
| 2026-01-26 | 6df1e9af2a | PARTIAL | DMAP + MAIR fix - enters page loop but hangs |
| 2026-01-26 | 2dcd46ccf9 | PARTIAL | ALIST fix - passes vm_page_startup, crashes in kmem_init |
| 2026-01-26 | ea388dc650 | PARTIAL | Debug cleanup - same state, cleaner output |
| 2026-01-26 | d3d859db2f | PARTIAL | -ffixed-x18 flag - passes kmem_init, crashes in SI_BOOT1_ALLOCATOR |
| 2026-01-26 | ad57b7bd56 | PARTIAL | pmap_enter() implementation - passes SI_BOOT1_ALLOCATOR |
| 2026-01-27 | eeb2db1d22 | PARTIAL | Timer init order fix - reaches SI_BOOT2_START_CPU |
| 2026-01-27 | 1b68068698 | PARTIAL | cpu_startup() implementation - reaches context switch |
| 2026-01-27 | 042add5899 | PARTIAL | cpu_lwkt_switch() implementation - passes context switch |
| 2026-01-27 | 716bd835b4 | PARTIAL | cpu_set_thread_handler() - devfs thread creation works |
| 2026-01-27 | 1f5a1b62ac | PARTIAL | Cache aliasing fix - slab allocator working |
| 2026-01-27 | a6064ef072 | PARTIAL | pmap_kvtom() implementation - kfree() works |
| 2026-01-27 | a0eca92bfd | **SUCCESS** | High VA jump after MMU enable - **boots to mountroot** |

## Files Modified

### Core Fixes
- `sys/cpu/aarch64/include/cpufunc.h` - Memory barrier fix (dmb instructions)
- `sys/sys/alist.h` - ALIST_RECORDS buffer size fix
- `sys/platform/arm64/conf/kern.mk` - Added -ffixed-x18 and -mgeneral-regs-only

### DMAP/Memory Attribute Fixes
- `sys/platform/arm64/include/vm.h` - ARM64-specific VM_MEMATTR definitions
- `sys/platform/arm64/include/pte.h` - Changed VM_MEMATTR to MAIR_IDX constants
- `sys/platform/arm64/aarch64/locore.s` - Fixed MAIR_VALUE and PTE block flags
- `sys/platform/arm64/aarch64/machdep.c` - Fixed PTE_BLOCK_NORMAL_FLAGS

### Debug (removed)
- `sys/vm/vm_page.c` - Debug output (removed in ea388dc650)
- `sys/kern/subr_alist.c` - Debug output (removed in ea388dc650)

## Related Files

- `sys/sys/spinlock2.h` - Uses `cpu_sfence()` in `spin_unlock_quick()`
- `sys/platform/arm64/include/thread.h` - Defines `mycpu` macro reading x18
- `sys/platform/arm64/aarch64/machdep.c` - Sets x18 in `arm64_init_globaldata()`

## QMP Debugging Commands

The Makefile supports QMP for live debugging:

```bash
# Terminal 1: Start QEMU with QMP
make debug

# Terminal 2: Query registers
make qmp-regs

# Other commands
make qmp-stop    # Pause VM
make qmp-cont    # Resume VM
make qmp-quit    # Terminate QEMU
```

## References

- FreeBSD `sys/conf/kern.mk` lines 142-155 - ARM64 kernel CFLAGS including `-ffixed-x18`
- FreeBSD `sys/arm64/include/atomic.h` - Proper ARM64 barrier usage
- ARM Architecture Reference Manual - Memory barrier instructions
- `doc/arm64-efi-loader-mvp-part1.md` - Main ARM64 port documentation (all MVP parts complete through Part 13)

---

*Last updated: 2026-01-28 (All issues resolved - kernel boots to mountroot)*
