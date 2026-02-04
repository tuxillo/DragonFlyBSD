# LinuxKPI Phase 3 – VM Mapping (SG Pager + Memattr) Tests

This suite exercises the LKPI mmap path so it matches expected behavior in
`~/s/drm-kmod`: userspace calls `mmap(2)` on a DRM-style LKPI cdev, the
`file_operations.mmap` hook is invoked, a LinuxKPI `vm_area_struct` is prepared,
and DragonFly returns a valid VM object with the requested caching attributes.

This is a full plan for Suite 3 implementation. It is intentionally detailed
and prescriptive so the test matches drm-kmod behavior and validates the
critical LKPI mmap plumbing on DragonFly.

## Target behavior (match drm-kmod)

- Mapping is offset-based: the test uses an ioctl to get a fake mmap offset,
  then passes that offset to `mmap(2)`.
- The file ops `.mmap` method is the entry point, similar to drm-kmod's
  `.mmap = drm_gem_mmap` or driver-specific mmap functions.
- The cache mode is controlled by `vma->vm_page_prot` and must be reflected in
  the resulting VM object memattr (WC/UC/WB).
- Mappings are shared; `MAP_PRIVATE` is rejected for these device mappings.

## Components (new)

- Userland driver: `test/testcases/linuxkpi/mmap/linuxkpi_mmap`
- Shared ABI: `test/testcases/linuxkpi/mmap/lkpi_mmaptest.h`
- Kernel module: `test/testcases/linuxkpi/mmap/kmod/lkpi_mmaptest_kmod.ko`
- Runlist: `test/testcases/linuxkpi_mmap.run`

## IOCTL ABI (proposed)

All ioctls are ASCII stable and return `-errno` semantics in kernel.

```c
/* lkpi_mmaptest.h */

#define LKPI_MMAPTEST_ALLOC       _IOWR('M', 0x01, struct lkpi_mmap_alloc)
#define LKPI_MMAPTEST_FREE        _IOW('M', 0x02, struct lkpi_mmap_free)
#define LKPI_MMAPTEST_QUERY       _IOWR('M', 0x03, struct lkpi_mmap_query)
#define LKPI_MMAPTEST_GET_MEMATTR _IOWR('M', 0x04, struct lkpi_mmap_memattr)

enum lkpi_mmap_cache {
    LKPI_MMAP_CACHE_WB = 0,
    LKPI_MMAP_CACHE_WC = 1,
    LKPI_MMAP_CACHE_UC = 2,
};

struct lkpi_mmap_alloc {
    uint64_t size;
    uint32_t cache;      /* enum lkpi_mmap_cache */
    uint32_t flags;      /* reserved */
    uint64_t mmap_off;   /* byte offset for mmap(2) */
    uint64_t handle;     /* kernel handle for bookkeeping */
};

struct lkpi_mmap_free {
    uint64_t handle;
};

struct lkpi_mmap_query {
    uint64_t handle;
    uint64_t size;
    uint32_t cache;      /* enum lkpi_mmap_cache */
    uint32_t flags;      /* reserved */
    uint64_t mmap_off;
};

struct lkpi_mmap_memattr {
    uint64_t handle;
    uint32_t memattr;    /* VM_MEMATTR_* */
    uint32_t flags;      /* reserved */
};
```

## Kernel module behavior (required)

The kernel module simulates a drm-kmod GEM-style mapping flow without touching
drm-kmod. It maintains a small map of mmap objects keyed by `mmap_off`/handle.

### State per mapping object

- `handle` (stable ID for ioctl lookups)
- `mmap_off` (page-aligned fake offset)
- `size` (page-aligned)
- `cache_type` (WB/WC/UC)
- `phys_base` or `pfn_base` for SG-mode backing
- `vma_ops` / `vm_private_data` for vm_ops mode
- refcounts and open/close tracking

### Allocation ioctl (LKPI_MMAPTEST_ALLOC)

- Validates size (page aligned, non-zero, sane upper bound).
- Allocates backing memory for SG mode:
  - Use contig/phys allocator (simple, deterministic PFN base).
  - Fill with a known pattern (`0x5a`, `0xa5`, or increasing bytes).
- Assigns a new `mmap_off` (page-aligned fake offset).
- Returns `handle` and `mmap_off` to userland.

### Mmap hook

`int mmap(struct linux_file *filp, struct vm_area_struct *vma)` should:

- Look up the object by `vma->vm_pgoff` (fake offset in pages).
- Validate size and bounds (`vma->vm_end - vma->vm_start == obj->size`).
- Reject `MAP_PRIVATE` and other invalid flags (EINVAL).
- Set `vma->vm_page_prot` based on requested cache type:
  - WB: `vm_get_page_prot(vma->vm_flags)`
  - WC: `pgprot_writecombine(vm_get_page_prot(vma->vm_flags))`
  - UC: `pgprot_noncached(vm_get_page_prot(vma->vm_flags))`
- Choose one of the two mapping modes below.

### Mapping mode A: SG pager path (mandatory)

This exercises the existing LKPI SG pager path and memattr propagation on
DragonFly.

- Set `vma->vm_pfn` to the backing PFN base.
- Set `vma->vm_len = obj->size`.
- Set `vma->vm_ops = NULL`.

Result: LKPI `linux_file_mmap_single()` takes the SG path and calls
`cdev_pager_allocate()` with `lkpi_sg_pager_ops`. The resulting object should
have `memattr` updated based on `vma->vm_page_prot`.

### Mapping mode B: vm_ops fault path (DRM-like, planned)

This mirrors drm-kmod GEM/TTM usage where the driver supplies `vm_ops` and
faults pages in on demand.

- Set `vma->vm_ops` to a module-defined `vm_operations_struct` with
  `.open/.close/.fault`.
- Set `vma->vm_private_data` to a stable per-object handle.
- `.fault` uses LKPI helpers (e.g. `vmf_insert_pfn_prot`) to insert pages.

Note: DragonFly's device pager uses `cdev_pg_fault` and currently does not
invoke `vm_ops->fault` from the pager fault path. Suite 3 should land in two
phases:

1) SG pager mode first (expected to work immediately).
2) vm_ops mode after LKPI pager fault behavior is extended on DragonFly to call
   the LinuxKPI vm_ops->fault handler. That change must be validated here.

## Userland test flow (required)

### Main success path

1. `open("/dev/lkpi_mmaptest", O_RDWR)`
2. `ioctl(ALLOC)` with size and cache type, capture `mmap_off` + `handle`.
3. `mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, mmap_off)`
4. Verify:
   - bytes read match expected pattern
   - writes are visible (write through mapping, then read via ioctl or
     a second mapping)
5. `ioctl(GET_MEMATTR)` to validate cache mode propagation.
6. `munmap()` and `ioctl(FREE)`.

### Required negative tests

- `MAP_PRIVATE` must fail with `EINVAL`.
- invalid offset (non-existing `mmap_off`) must fail with `EINVAL`.
- unaligned offset must fail with `EINVAL`.

## Memattr verification (required)

The test must verify that cache-mode settings are actually reflected in the
VM object memattr.

Kmod implements `GET_MEMATTR(handle)`:

- `obj = cdev_pager_lookup(handle)`
- return `obj->memattr` to userland

Expected mapping:

- WB -> `VM_MEMATTR_DEFAULT`
- WC -> `VM_MEMATTR_WRITE_COMBINING` (or UC if WC unavailable)
- UC -> `VM_MEMATTR_UNCACHEABLE`

## Pass criteria

- mmap succeeds and returns a usable mapping for `MAP_SHARED`.
- page faults succeed without SIGBUS/panic.
- writes through the mapping persist.
- memattr matches requested cache mode.
- `MAP_PRIVATE` fails with `EINVAL`.
- unload is clean (no leaks/panics/hangs).

## Run (planned)

```sh
dfregress test/testcases/linuxkpi_mmap.run
```
