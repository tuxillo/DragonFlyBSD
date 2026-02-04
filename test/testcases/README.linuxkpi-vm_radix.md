# LinuxKPI Phase 3 – vm_radix Iterator Tests

This suite validates the FreeBSD-style vm_radix iterator surface implemented
over DragonFly’s `rb_memq` resident-page tree. It is designed to match the
LinuxKPI contract used by drm-kmod for page iteration semantics and boundary
conditions.

## Goal

Validate the LKPI-provided FreeBSD-style vm_radix/pctrie iterator surface
implemented over DragonFly’s `vm_object->rb_memq` resident-page RB tree:

- `struct pctrie_iter` state management
- lookup helpers: `vm_radix_iter_lookup*()` and `vm_radix_rb_lookup_{ge,le}()`
- iteration macros:
  - `VM_RADIX_FOREACH*` (skip holes; uses RB_NEXT via `vm_radix_iter_step`)
  - `VM_RADIX_FORALL*` (consecutive only; uses `vm_page_next` which enforces
    `pindex+1`)

## Files to add (new suite)

- `test/testcases/linuxkpi/vm_radix/Makefile` (`.MAIN: all` + `.PHONY: kmod` like
  Suites 2–4)
- `test/testcases/linuxkpi/vm_radix/lkpi_vm_radix.h` (ioctl ABI + names)
- `test/testcases/linuxkpi/vm_radix/linuxkpi_vm_radix.c` (userland: pre/load,
  run/ioctl, post/unload)
- `test/testcases/linuxkpi/vm_radix/kmod/Makefile`
- `test/testcases/linuxkpi/vm_radix/kmod/lkpi_vm_radixtest_kmod.c` (kernel tests
  + `/dev` node)
- `test/testcases/linuxkpi_vm_radix.run`

## Wire-in / docs

- Add `SUBDIR+= vm_radix` to `test/testcases/linuxkpi/Makefile`
- Update `test/testcases/README.linuxkpi.md` Suite 5 run line to:
  `dfregress test/testcases/linuxkpi_vm_radix.run`
- Update this README from “planned” to “implemented” and fix the run command

## Kernel module test design

- Create a test device `/dev/lkpi_vmradix` (or similar) via LKPI `cdev_alloc()` /
  `cdev_init()` / `kobject_set_name()` / `cdev_add()`
- Provide one ioctl: `LKPI_VMRADIX_RUN` returning a small result struct:
  - `status` (0 pass / 1 fail)
  - `subtest` id
  - `err` (BSD errno)

## How the kmod sets up data

- Create two `vm_object_t`s and populate them with pages using DragonFly VM APIs
  while holding the object token (as required by `vm_radix.h`):
  - `obj_sparse`: pages at indices `{0, 2, 10}`
  - `obj_dense`: pages at indices `{10, 11, 12}` (to exercise the “consecutive”
    iterator)
- Population approach (under `vm_object_hold(obj)`):
  - `m = vm_page_grab(obj, pindex, VM_ALLOC_NORMAL | VM_ALLOC_RETRY)`
  - ensure page is usable for lookup/iteration, then `vm_page_wakeup(m)` to drop
    busy
- At end: `vm_object_drop(obj)` and `vm_object_deallocate(obj)` (object
  termination cleans up pages)

## Subtests to implement inside RUN

1) **FOREACH skips holes (sparse object)**
   - `vm_page_iter_init(&it, obj_sparse)`
   - `VM_RADIX_FOREACH(m, &it)` should visit pindex sequence: `0, 2, 10`
   - Also validate `VM_RADIX_FOREACH_FROM(..., start=1)` visits `2, 10`
   - Validate `...start=3` visits `10`
   - Validate `...start=11` visits none

2) **FORALL stops at first hole (sparse object)**
   - `VM_RADIX_FORALL_FROM(..., start=0)` yields only `0` (because
     `vm_page_next(0)` returns NULL when next pindex isn’t 1)
   - `...start=2` yields only `2`

3) **FORALL walks consecutive pages (dense object)**
   - `VM_RADIX_FORALL_FROM(..., start=10)` yields `10,11,12`
   - `...start=11` yields `11,12`
   - `...start=9` yields none (because `vm_radix_iter_lookup` is exact)

4) **Lookup helper semantics (sparse object)**
   - `vm_radix_iter_lookup_ge(&it, 9)` returns 10
   - `vm_radix_iter_lookup_le(&it, 9)` returns 2
   - `vm_radix_iter_lookup_lt(&it, 1)` returns 0
   - `vm_radix_iter_lookup_lt(&it, 0)` returns NULL

5) **Step/prev walk RB order (sparse object)**
   - `vm_radix_iter_lookup_ge(&it, 0)` then repeated `vm_radix_iter_step(&it)`
     yields `0 -> 2 -> 10 -> NULL`
   - `vm_radix_iter_lookup_le(&it, 10)` then `vm_radix_iter_prev(&it)` yields
     `10 -> 2 -> 0 -> NULL`

6) **Limit behavior**
   - `vm_page_iter_limit_init(&it, obj_sparse, limit=2)`
   - `VM_RADIX_FOREACH(m, &it)` visits only `0,2`
   - `vm_radix_iter_lookup_ge(&it, 3)` returns NULL due to limit clamp

## Userland runner

- Same “pre/run/post” model as the other suites:
  - `pre`: kldload `kmod/lkpi_vm_radixtest_kmod.ko` (also search
    `kmod/obj/...`)
  - `run`: open `/dev/lkpi_vmradix`, ioctl RUN, assert pass, print
    `vm_radix test passed`
  - `post`: kldunload

## What this will tell us

- Whether the vm_radix surface behaves like FreeBSD’s contract from the
  perspective of LKPI consumers:
  - RB-based “next existing page” iteration for FOREACH
  - strict consecutive iteration for FORALL via `vm_page_next`
  - boundary and limit correctness (common source of subtle bugs)

## Run

```sh
dfregress test/testcases/linuxkpi_vm_radix.run
```
