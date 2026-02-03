# LinuxKPI Phase 3 – vm_radix Iterator Tests

This suite validates the FreeBSD-style vm_radix iterator surface implemented
over DragonFly’s `rb_memq` resident-page tree.

## What it covers

- `struct pctrie_iter` + iterator helpers
- `VM_RADIX_FOREACH` (all pages, skip holes)
- `VM_RADIX_FORALL` (consecutive pages)

## Test shape (planned)

- Kernel module creates a vm_object with pages at indices 0, 2, 10
- Validates:
  - `VM_RADIX_FOREACH` visits 0,2,10
  - `VM_RADIX_FORALL` from 0 stops at 0
  - `lookup_ge`/`step` behavior matches expectations

## Pass criteria

- iteration order and stop conditions are correct
- no panics/assertions under object lock

## Run (planned)

```sh
dfregress -r linuxkpi_vm_radix.run
```
