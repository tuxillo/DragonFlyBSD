# LinuxKPI DragonFly Compatibility Notes

This document tracks LinuxKPI compatibility items that require deeper
DragonFly integration or careful shims beyond simple symbol mapping.

## VM Compatibility: vm_pager_allocate / vm_object_set_memattr

### Status (Implemented)

The DragonFly compatibility layer now implements:

- an SG pager shim using `cdev_pager_allocate()` with custom ops
- a local `vm_object_set_memattr()` wrapper that updates resident pages
- LinuxKPI mmap path wiring to use the SG pager shim on DragonFly

Implementation locations:
- `sys/compat/linuxkpi/common/src/linux_compat.c`

### Background and Problem Statement

LinuxKPI references two FreeBSD VM APIs that are not present in DragonFly:

- `vm_pager_allocate(objtype_t type, void *handle, vm_ooffset_t size,
  vm_prot_t prot, vm_ooffset_t off, struct ucred *cred)`
- `vm_object_set_memattr(vm_object_t object, vm_memattr_t memattr)`

DragonFly does not implement these interfaces. The LinuxKPI mmap path
calls them for scatter/gather mappings and for setting object memory
attributes during device mappings. A direct port of FreeBSD behavior is
not available because DragonFly lacks:

- A `vm_pager_allocate()` dispatcher
- An `OBJT_SG` VM object type
- A `vm_object_set_memattr()` setter with FreeBSD semantics

### FreeBSD Reference (authoritative behavior)

1) **vm_pager_allocate**

FreeBSD defines a generic dispatcher in `~/s/freebsd/sys/vm/vm_pager.c`:

```c
vm_object_t
vm_pager_allocate(objtype_t type, void *handle, vm_ooffset_t size,
    vm_prot_t prot, vm_ooffset_t off, struct ucred *cred)
{
    vm_object_t object;

    MPASS(type < nitems(pagertab));

    object = (*pagertab[type]->pgo_alloc)(handle, size, prot, off, cred);
    if (object != NULL)
        object->type = type;
    return (object);
}
```

This supports `OBJT_SG` (scatter/gather) via the SG pager.

2) **OBJT_SG**

`OBJT_SG` is a real object type in FreeBSD (`~/s/freebsd/sys/vm/vm.h`) and
is implemented by `~/s/freebsd/sys/vm/sg_pager.c`.

Key `sg_pager.c` behaviors:
- validates `foff` and sglist page alignment
- computes the page count based on sg segments
- allocates a new object of type `OBJT_SG`
- holds the sglist (`sglist_hold`) so the caller can free its reference
- on fault, maps offsets to physical addresses and returns fictitious pages

`sg_pager_getpages()` replaces the requested page with a fake page
created by `vm_page_getfake()` and marks it valid.

3) **vm_object_set_memattr**

FreeBSD defines this in `~/s/freebsd/sys/vm/vm_object.c`:

```c
int
vm_object_set_memattr(vm_object_t object, vm_memattr_t memattr)
{
    VM_OBJECT_ASSERT_WLOCKED(object);

    if (object->type == OBJT_DEAD)
        return (KERN_INVALID_ARGUMENT);
    if (!vm_radix_is_empty(&object->rtree))
        return (KERN_FAILURE);

    object->memattr = memattr;
    return (KERN_SUCCESS);
}
```

Semantics:
- must be called while holding the object lock
- must be called before any pages are allocated
- returns KERN_* status

### DragonFly Current State

DragonFly provides a set of pager allocators in `sys/vm/vm_pager.h`:

- `swap_pager_alloc()`
- `dev_pager_alloc()`
- `phys_pager_alloc()`
- `cdev_pager_allocate()` (device-pager with custom ops)

There is no `vm_pager_allocate()` dispatcher and no `OBJT_SG` object type
in `sys/vm/vm_object.h`.

DragonFly `struct vm_object` contains a `vm_memattr_t memattr` field, but
there is no `vm_object_set_memattr()` function.

### LinuxKPI Call Sites

In DragonFly LinuxKPI, `vm_pager_allocate(OBJT_SG, ...)` and
`vm_object_set_memattr()` are used in the LinuxKPI mmap path for device
memory mappings and scatter/gather mappings:

`sys/compat/linuxkpi/common/src/linux_compat.c`:

```c
sg = sglist_alloc(1, M_WAITOK);
sglist_append_phys(sg,
    (vm_paddr_t)vmap->vm_pfn << PAGE_SHIFT, vmap->vm_len);

#ifdef __DragonFly__
sg_handle = kmalloc(sizeof(*sg_handle), M_LKPI_SG_HANDLE,
    M_WAITOK | M_ZERO);
sg_handle->sg = sg;
sg_handle->len = vmap->vm_len;
*object = cdev_pager_allocate(sg_handle, OBJT_DEVICE,
    &lkpi_sg_pager_ops, vmap->vm_len, nprot, 0, td->td_ucred);
if (*object != NULL)
    sglist_free(sg);
#else
*object = vm_pager_allocate(OBJT_SG, sg, vmap->vm_len,
    nprot, 0, td->td_ucred);
#endif

...
if (attr != VM_MEMATTR_DEFAULT) {
    VM_OBJECT_WLOCK(*object);
    vm_object_set_memattr(*object, attr);
    VM_OBJECT_WUNLOCK(*object);
}
```

### Detailed Plan (DragonFly-Compatible Implementation)

#### 1) Provide a DragonFly SG-pager shim using `cdev_pager_allocate()`

Goal: Replace `OBJT_SG` usage with a DragonFly-compatible pager that maps
scatter/gather lists to device-backed VM objects. (Implemented.)

Approach:
- Implement a LinuxKPI-local “SG pager” on DragonFly using existing
  `cdev_pager_allocate()` infrastructure.
- This avoids adding `OBJT_SG` to DragonFly’s core VM object types.

Proposed design:

**Data structure:**
```c
struct lkpi_sg_handle {
    struct sglist *sg;
    vm_ooffset_t len;
};
```

**Pager ops:**
```c
static struct cdev_pager_ops lkpi_sg_pager_ops = {
    .cdev_pg_ctor = lkpi_sg_pager_ctor,
    .cdev_pg_dtor = lkpi_sg_pager_dtor,
    .cdev_pg_fault = lkpi_sg_pager_fault,
};
```

**Constructor (lkpi_sg_pager_ctor):**
- validate `foff` is page-aligned
- validate each `sg->sg_segs[i]` is page-aligned and length is page-multiple
- confirm `foff + size` is within the sglist total length
- take ownership via `sglist_hold()`
- store `len` for bounds checks
- return 0 on success, non-zero on failure

**Fault handler (lkpi_sg_pager_fault):**
- translate `offset` to `paddr` by walking `sg->sg_segs[]` like FreeBSD
  `sg_pager_getpages()` (use an accumulator `space`)
- construct or update a fictitious page:
  - if `*mres` is fictitious, update with `vm_page_updatefake()`
  - else create a new fake page via `vm_page_getfake()` and replace
    the old page (`vm_page_replace()`)
- set `page->valid = VM_PAGE_BITS_ALL`
- insert fake page into `object->un_pager.devp.devp_pglist` so
  `device_pager` dealloc can free it
- return `VM_PAGER_OK` or `VM_PAGER_FAIL`

**Destructor (lkpi_sg_pager_dtor):**
- `sglist_free(handle->sg)`
- free the `lkpi_sg_handle`

**Allocation path:**
- replace `vm_pager_allocate(OBJT_SG, sg, ...)` in DragonFly LinuxKPI
  with:

```c
handle = kmalloc(sizeof(*handle), M_LKPI_SG, M_WAITOK | M_ZERO);
handle->sg = sg;
handle->len = vmap->vm_len;

object = cdev_pager_allocate(handle, OBJT_DEVICE,
    &lkpi_sg_pager_ops, vmap->vm_len, nprot, 0, td->td_ucred);
```

**Reference handling:**
- When the pager takes a `sglist_hold()`, the caller should drop its
  original reference on success (mirror FreeBSD usage):

```c
if (object != NULL)
    sglist_free(sg);
```

This ensures the sglist is freed when the object is deallocated.

**Why OBJT_DEVICE:**
- `device_pager` already frees `object->un_pager.devp.devp_pglist`
- aligns with how DragonFly device mappings already work

#### 2) Implement a DragonFly-compatible `vm_object_set_memattr()` wrapper

Goal: Provide the FreeBSD behavior for LinuxKPI usage without modifying
DragonFly core VM. (Implemented, with resident-page updates.)

Proposed wrapper (LinuxKPI-local, `__DragonFly__` only):

```c
static inline int
vm_object_set_memattr(vm_object_t object, vm_memattr_t memattr)
{
    VM_OBJECT_ASSERT_WLOCKED(object); /* or best-effort check */

    if (object->type == OBJT_DEAD)
        return (KERN_INVALID_ARGUMENT);

    object->memattr = memattr;
    if (object->resident_page_count != 0) {
        for (page = RB_MIN(vm_page_rb_tree, &object->rb_memq);
             page != NULL;
             page = RB_NEXT(vm_page_rb_tree, &object->rb_memq, page))
            pmap_page_set_memattr(page, memattr);
    }
    return (KERN_SUCCESS);
}
```

Notes:
- DragonFly uses `vm_object_hold/drop` as lock shims in LinuxKPI
  (`VM_OBJECT_WLOCK/UNLOCK` in `linux/mm.h`), so this check is
  a best-effort compatibility layer.
- FreeBSD checks `vm_radix_is_empty(&object->rtree)`; DragonFly has no
  `vm_radix` in `vm_object`, so the wrapper updates any resident pages
  and returns success.
- Return codes should use `KERN_SUCCESS/KERN_FAILURE/KERN_INVALID_ARGUMENT`
  where available; if those are not defined, map to `0/EINVAL/ENOMEM` and
  update LinuxKPI call sites accordingly.

#### 3) Adjust LinuxKPI callers to use the new DragonFly shims

In `linux_compat.c` (DragonFly branch):
- Replace `vm_pager_allocate(OBJT_SG, ...)` call with the new
  `cdev_pager_allocate()` + sg-pager shim path. (Implemented.)
- Use the new `vm_object_set_memattr()` wrapper for memattr assignment.
  (Implemented.)
- Ensure sglist refcount is handled consistently (free the initial
  reference after successful object allocation). (Implemented.)

#### 4) OBJT_SG macro handling in LinuxKPI

Currently `OBJT_SG` is mapped to `OBJT_DEFAULT` in
`sys/compat/linuxkpi/common/include/linux/dragonfly_compat.h`.

Plan:
- Avoid using `OBJT_SG` on DragonFly entirely by routing the LinuxKPI
  mapping path to the new sg-pager shim. (Implemented.)
- Keep the `OBJT_SG` macro only as a compile-time placeholder but do not
  rely on it for behavior.

### Validation Checklist

1) Build kernel (quickkernel) after adding the wrappers. (Pending)
2) Confirm `linux_compat.c` no longer references missing symbols. (Pending)
3) Run workqueue tests only if unrelated changes did not destabilize build.
4) When drm-kmod is ready, validate:
- device mapping via DRM nodes works
- mmap of device memory returns valid mappings

### Risks and Considerations

- The SG pager shim must carefully manage fictitious page lifetime to
  avoid leaks or UAF (use `devp_pglist`).
- `vm_object_set_memattr()` cannot fully emulate FreeBSD behavior without
  a radix tree; the `resident_page_count` guard is a best-effort check.
- `cdev_pager_allocate()` ctor/dtor must be MPSAFE and avoid sleeping
  in unsafe contexts.

## VM Compatibility: vm_radix (iterator surface)

### Status (Implemented)

DragonFly now provides a FreeBSD-style vm_radix iterator surface via
`sys/compat/linuxkpi/common/include/vm/vm_radix.h`, implemented over
DragonFly’s `vm_object->rb_memq` resident page RB tree.

### Why it is needed

FreeBSD’s VM uses `vm_radix` (pctrie + SMR) for resident page indexing.
DragonFly uses an RB tree (`rb_memq`) and does not provide vm_radix APIs.
LinuxKPI and FreeBSD-derived code paths expect macros like
`VM_RADIX_FORALL` and iterator helpers (`vm_page_iter_init`,
`vm_radix_iter_lookup_ge`, etc.).

### Implementation notes

Implemented in `sys/compat/linuxkpi/common/include/vm/vm_radix.h`:

- `struct pctrie_iter` compatibility cursor
- `vm_page_iter_init`, `vm_page_iter_limit_init`, `pctrie_iter_reset`
- `vm_radix_iter_lookup[_ge/_le/_lt]`, `vm_radix_iter_next`,
  `vm_radix_iter_step`, `vm_radix_iter_prev`
- `VM_RADIX_FORALL[_FROM]` (consecutive pages using `vm_page_next`)
- `VM_RADIX_FOREACH[_FROM]` (full iteration using RB in-order traversal)

### Locking contract

Callers must hold the `vm_object` lock while iterating or performing
lookups. The iterator is not safe against concurrent insert/remove
without the object lock held.

### Build selection

No drm-kmod changes are made here. When drm-kmod builds are enabled,
ensure the build selects the vm_radix iterator path without source edits
(for example, by providing an appropriate `__FreeBSD_version` define in
the drm-kmod build flags).
