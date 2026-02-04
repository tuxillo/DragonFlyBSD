# LinuxKPI Phase 3 – sf_buf + highmem + scatterlist Tests

This suite validates the LinuxKPI scatterlist and highmem APIs used by DRM
drivers and DMA code paths, with an emphasis on correct SG table construction,
iteration, and buffer copying. It exercises both kmap/kunmap operations and
scatterlist helpers such as `sg_alloc_table_from_pages()` and
`sg_copy_{to,from}_buffer()`.

## What it covers

- highmem helpers: `kmap`, `kunmap`, `kmap_local_page`, `kunmap_local`,
  `memcpy_to_page`, `memcpy_from_page`
- scatterlist helpers:
  - `sg_init_table`, `sg_set_page`, `sg_mark_end`, `sg_next`
  - `sg_alloc_table_from_pages`, `sg_free_table`, `sg_nents`
  - `sg_copy_from_buffer`, `sg_copy_to_buffer`, `sg_pcopy_to_buffer`
- chained scatterlist allocation path (`sg_alloc_table` with > SG_MAX_SINGLE_ALLOC)

## Test shape

- Kernel module registers a test char device and exposes a single RUN ioctl
- Userland helper loads the module, triggers the RUN ioctl, and checks results

### Components

- Userland driver: `test/testcases/linuxkpi/scatterlist/linuxkpi_scatterlist`
- Kernel module: `test/testcases/linuxkpi/scatterlist/kmod/lkpi_sgtest_kmod.ko`
- Device node: `/dev/lkpi_sgtest`

### Subtests

1) **Highmem round-trip**
   - write patterns via `memcpy_to_page` and `kmap_local_page`
   - verify via `memcpy_from_page`

2) **sg_table from pages**
   - `sg_alloc_table_from_pages()` with non-zero offset and size
   - verify offsets/lengths and total byte count

3) **Scatterlist copy helpers**
   - `sg_copy_from_buffer()` + `sg_copy_to_buffer()`
   - `sg_pcopy_to_buffer()` with non-zero skip

4) **Chained sg list iteration**
   - `sg_alloc_table()` with > SG_MAX_SINGLE_ALLOC entries
   - verify `for_each_sg()` walks all entries

## Pass criteria

- RUN ioctl reports success for all subtests
- No leaks/panics/hangs on unload

## Status

- Sufficient baseline coverage for Phase 3 scatterlist/highmem work.
- DMA-mapping and additional iterator/stress coverage can be added later.

## Notes

- Depends on correct LKPI page allocation/free semantics on DragonFly
  (`linux_alloc_pages()`/`linux_free_pages()` correctness).

## Run

```sh
dfregress test/testcases/linuxkpi_scatterlist.run
```
