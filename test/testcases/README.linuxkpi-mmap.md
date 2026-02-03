# LinuxKPI Phase 3 – VM Mapping (SG Pager + Memattr) Tests

This suite validates the LinuxKPI mmap path backed by the DragonFly SG pager
shim and the `vm_object_set_memattr()` behavior.

## What it covers

- SG pager shim based on `cdev_pager_allocate()`
- mmap of an SG-backed region
- `vm_object_set_memattr()` updating resident pages

## Test shape (planned)

- Kernel module provides an mmap-capable cdev mapping a synthetic SG region
- Userland helper:
  - maps the region
  - touches pages to fault them in
  - triggers a memattr change via ioctl and re-touches

## Pass criteria

- mmap succeeds and returns a usable mapping
- page faults succeed without SIGBUS/panic
- memattr change does not break the mapping

## Run (planned)

```sh
dfregress -r linuxkpi_mmap.run
```
