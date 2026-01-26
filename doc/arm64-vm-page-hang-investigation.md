# ARM64 VM Page Startup Hang Investigation

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

### Issue 3: x18 Register Clobbering (Current)

**Status:** Fix identified, implementation pending.

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

**Fix:** Add `-ffixed-x18` to kernel CFLAGS in `sys/platform/arm64/conf/kern.mk`, following FreeBSD's pattern (`.freebsd.orig/sys/conf/kern.mk:146`).

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

### Session 4: x18 Register Investigation

1. Kernel crashes in `kmem_init()` with DABT exception
2. Fault address `0x20003544` = corrupted x18 + offset
3. Used objdump to find compiler writes to x18
4. Identified missing `-ffixed-x18` flag

## Test Results

| Date | Commit | Result | Notes |
|------|--------|--------|-------|
| 2026-01-26 | a7edc9d061 | PARTIAL | Memory barrier fix - system reaches vm_page_startup |
| 2026-01-26 | 6df1e9af2a | PARTIAL | DMAP + MAIR fix - enters page loop but hangs |
| 2026-01-26 | 2dcd46ccf9 | PARTIAL | ALIST fix - passes vm_page_startup, crashes in kmem_init |
| 2026-01-26 | ea388dc650 | PARTIAL | Debug cleanup - same state, cleaner output |
| 2026-01-26 | TBD | PENDING | -ffixed-x18 flag to reserve x18 register |

## Files Modified

### Core Fixes
- `sys/cpu/aarch64/include/cpufunc.h` - Memory barrier fix (dmb instructions)
- `sys/sys/alist.h` - ALIST_RECORDS buffer size fix
- `sys/platform/arm64/conf/kern.mk` - Add -ffixed-x18 (pending)

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
