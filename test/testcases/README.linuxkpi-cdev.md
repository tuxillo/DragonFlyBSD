# LinuxKPI Phase 3 – LKPI cdev/devfs Bridge Tests

This suite exercises the LinuxKPI cdev registration and the DragonFly
devfs/fileops bridge for DRM-style device nodes. It focuses on per-open
file behavior, ioctl/uaccess plumbing, and kqueue readiness propagation.

## What it covers (implemented)

- `cdev_add()` / `cdev_add_ext()` devfs node creation
- `linuxdev_ops` bridge for open/close/read/write/ioctl/kqfilter
- `linux_dev_fdopen()` wiring (vnode-shaped fd with LKPI per-open storage)
- per-open file lifetime and inherited fd behavior (dup/fork)
- LKPI poll -> kqueue readiness propagation via `linux_poll_wakeup()`
- ioctl/uaccess round-trip, invalid ioctl behavior

## Test shape

- Kernel module registers a test char device and exposes a small ioctl ABI
- Userland helper opens the device, issues ioctls, and validates read/write
- Poll readiness is driven via ioctl and observed via kqueue
- dup/fork behavior is validated on inherited file descriptors
- mmap behavior is validated by suite 3

### Components

- Userland driver: `test/testcases/linuxkpi/cdev/linuxkpi_cdev`
- Kernel module: `test/testcases/linuxkpi/cdev/kmod/lkpi_cdevtest_kmod.ko`
- Device node: `/dev/lkpi_cdevtest`

### Subtests

- basic open/ioctl/read/write round-trip
- dup(): duplicated fd shares the same LKPI file context
- fork(): child can issue ioctls on inherited fd
- kqueue readiness:
  - initial readiness must be empty
  - `POLL_SET` triggers EVFILT_READ/EVFILT_WRITE
  - `POLL_CLR` clears readiness
- invalid ioctl returns `EINVAL`

## Pass criteria

- `/dev/...` node appears and is openable
- ioctl/read/write behave correctly
- kqueue readiness behaves as expected
- dup/fork tests pass on inherited fds
- invalid ioctl returns `EINVAL`
- No leaks/panics/hangs on unload

## Not covered (by design)

- mmap/VM object behavior and cache attributes (Suite 3)
- DRM GEM/TTM object management (drm-kmod scope)
- dma-buf/PRIME paths (drm-kmod scope)

## Run

```sh
dfregress test/testcases/linuxkpi_cdev.run
```
