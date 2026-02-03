# LinuxKPI Phase 3 – LKPI cdev/devfs Bridge Tests

This suite exercises the LinuxKPI cdev registration and the DragonFly
devfs/fileops bridge.

## What it covers

- `cdev_add()` / `cdev_add_ext()` devfs node creation
- `linuxdev_ops` bridge for open/close/read/write/ioctl/mmap_single/kqfilter
- `linux_dev_fdopen()` and `linuxfileops` wiring

## Test shape

- Kernel module registers a test char device and exposes a small ioctl ABI
- Userland helper opens the device, issues ioctls, and validates read/write
- Optional mmap path is validated by suite 3

### Components

- Userland driver: `test/testcases/linuxkpi/cdev/linuxkpi_cdev`
- Kernel module: `test/testcases/linuxkpi/cdev/kmod/lkpi_cdevtest_kmod.ko`
- Device node: `/dev/lkpi_cdevtest`

## Pass criteria

- `/dev/...` node appears and is openable
- ioctl/read/write behave correctly
- No leaks/panics/hangs on unload

## Run

```sh
dfregress -r linuxkpi_cdev.run
```
