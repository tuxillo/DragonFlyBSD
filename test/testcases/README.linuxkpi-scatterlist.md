# LinuxKPI Phase 3 – sf_buf/highmem/scatterlist Tests

This suite exercises the sf_buf compatibility shim and pinned scatterlist
copy paths.

## What it covers

- `sf_buf_alloc/free/ref/kva` DMAP-first behavior
- `sg_pcopy_from_buffer()` pinned/NOWAIT path
- highmem/scatterlist helpers that depend on sf_buf

## Test shape (planned)

- Kernel module allocates pages and builds an sglist
- Calls `sg_pcopy_from_buffer()` under pinned context
- Validates copied bytes against a known pattern

## Pass criteria

- copy succeeds under pinned/NOWAIT constraints
- data integrity checks pass
- no sleep-in-atomic warnings

## Run (planned)

```sh
dfregress -r linuxkpi_scatterlist.run
```
