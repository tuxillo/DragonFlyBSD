# LinuxKPI Test Suite

This document indexes the LinuxKPI test suites used to validate the
DragonFly LinuxKPI port.

## Suites

1) **Workqueues**
- File: `test/testcases/README.linuxkpi-workqueues.md`
- Run: `dfregress test/testcases/linuxkpi_workqueue.run`

2) **LKPI cdev/devfs + fileops bridge**
- File: `test/testcases/README.linuxkpi-cdev.md`
- Run: `dfregress test/testcases/linuxkpi_cdev.run`

3) **VM mapping (SG pager + memattr)**
- File: `test/testcases/README.linuxkpi-mmap.md`
- Run: `dfregress test/testcases/linuxkpi_mmap.run`

4) **sf_buf + highmem + scatterlist**
- File: `test/testcases/README.linuxkpi-scatterlist.md`
- Run: `dfregress test/testcases/linuxkpi_scatterlist.run`

5) **vm_radix iterator surface**
- File: `test/testcases/README.linuxkpi-vm_radix.md`
- Run: `dfregress test/testcases/linuxkpi_vm_radix.run`

## Notes

- Suites 3–5 are Phase 3 coverage for LinuxKPI compatibility work.
- Each suite uses per-test kernel modules for isolation and debugging.
