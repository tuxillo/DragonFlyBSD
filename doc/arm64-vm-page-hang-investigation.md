# ARM64 VM Page Startup Hang Investigation

## Issue Summary

The arm64 kernel hangs during `vm_page_startup()` when adding physical pages to free queues using the `VM_PHYSSEG_SPARSE` implementation.

## Root Cause

**Broken memory barrier functions in `sys/cpu/aarch64/include/cpufunc.h`.**

The `cpu_sfence()` and `cpu_lfence()` functions were implemented as compiler-only fences:

```c
static __inline void cpu_sfence(void) { __asm __volatile("" ::: "memory"); }
static __inline void cpu_lfence(void) { __asm __volatile("" ::: "memory"); }
```

ARM64 has **weak memory ordering** - the CPU can reorder stores and loads. These compiler-only fences prevent compiler reordering but NOT hardware reordering. On x86_64 (strong memory model), this works. On ARM64, it causes corruption.

### Impact

- `spin_unlock_quick()` calls `cpu_sfence()` before releasing locks
- TAILQ operations modify multiple pointers that must be visible in order
- Without proper hardware barriers, pointer updates get reordered, corrupting queues
- The hang is timing-dependent: UART debug output acts as an implicit barrier

## Investigation Timeline

### Session 1: Initial Debugging

1. Added UART debug output to `vm_page_startup()` to trace progress
2. Discovered hang occurs during the page addition loop
3. Narrowed down to `vm_add_new_page()` function

### Session 2: Binary Search for Hang Location

1. Added granular debug output at each step in `vm_add_new_page()`:
   - Entry point
   - After `PHYS_TO_VM_PAGE()`
   - Before duplicate check
   - Before page init
   - Before queue assignment
   - Before `TAILQ_INSERT_HEAD`
   - After completion

2. **Key observation**: Pages completed successfully when debug output was enabled, but hung when debug was disabled or reduced.

3. Test results:
   - With debug for first 5 pages: hung after page 4
   - With debug for first 10 pages: hung after page 9
   - With debug for first 20 pages: hung after page 19
   - With debug for first 25 pages: expected to hang after page 24

4. **Conclusion**: Debug output (UART writes) was acting as a timing barrier, masking a memory ordering issue.

### Session 3: Root Cause Analysis

1. Compared DragonFly and FreeBSD VM implementations
2. Verified `vm_phys_init()` is called at correct time (after `vm_page_array` allocation)
3. Examined TAILQ macros - no barriers in the macros themselves
4. **Found the bug**: `cpu_sfence()` and `cpu_lfence()` are compiler-only fences

FreeBSD ARM64 uses:
- `dmb ishst` for store fence (wmb)
- `dmb ishld` for load fence (rmb)
- `dmb ish` for full fence (mb)

## The Fix

**File**: `sys/cpu/aarch64/include/cpufunc.h`

**Before**:
```c
static __inline void cpu_sfence(void) { __asm __volatile("" ::: "memory"); }
static __inline void cpu_lfence(void) { __asm __volatile("" ::: "memory"); }
```

**After**:
```c
static __inline void cpu_sfence(void) { __asm __volatile("dmb ishst" ::: "memory"); }
static __inline void cpu_lfence(void) { __asm __volatile("dmb ishld" ::: "memory"); }
```

## Test Results

| Date | Commit | Result | Notes |
|------|--------|--------|-------|
| 2026-01-26 | a7edc9d061 | PARTIAL | Memory barrier fix - system reaches vm_page_startup but hangs before "adding pages" message |

### Test Analysis (2026-01-26)

The memory barrier fix allowed progress:
- ✓ Kernel boots, console initializes
- ✓ init_param1/2, msgbufinit complete
- ✓ mi_startup runs SYSINITs through vm_mem_init
- ✗ Hangs inside vm_page_startup() BEFORE the page addition loop

The hang is now occurring **earlier** in vm_page_startup() - during segment registration or vm_page_array allocation, not during the TAILQ operations.

**Next investigation needed**: The issue is now in the early part of vm_page_startup():
1. Segment registration via vm_phys_add_seg()
2. vm_page_array allocation via pmap_map()
3. vm_phys_init() call

This may be a different bug that was previously masked.

## Files Modified During Investigation

- `sys/vm/vm_page.c` - Debug output (to be removed)
- `sys/vm/vm_phys.c` - Debug output (to be removed)
- `sys/cpu/aarch64/include/cpufunc.h` - Memory barrier fix

## Related Files

- `sys/sys/spinlock2.h` - Uses `cpu_sfence()` in `spin_unlock_quick()`
- `sys/sys/queue.h` - TAILQ macros
- `sys/vm/vm_phys.h` - Segment lookup functions

## References

- FreeBSD `sys/arm64/include/atomic.h` - Proper ARM64 barrier usage
- ARM Architecture Reference Manual - Memory barrier instructions
