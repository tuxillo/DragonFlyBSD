# DragonFly LinuxKPI Port for DRM/GPU Support

This document captures the porting effort to bring FreeBSD's LinuxKPI compatibility layer to DragonFly BSD for **DRM/GPU driver support only**.

## Project Scope

**PRIMARY OBJECTIVE:** Enable drm-kmod (external repository) to build and run on DragonFly BSD for modern GPU graphics drivers (Intel i915, AMD amdgpu/radeon).

**EXPLICIT NON-GOALS:**
- Wireless/WiFi driver support (802.11, cfg80211, mac80211)
- Network driver support (Mellanox, Chelsio, etc.)
- USB device driver support via LinuxKPI
- Modem/MHI driver support

The LinuxKPI port is **GPU/DRM-focused only**. All networking, wireless, USB, and modem-related code has been intentionally excluded or removed to reduce complexity and maintenance burden.

---

## Current Branch State (as of 2026-01-31)

**Branch:** `port-linuxkpi`
**Backup:** `backup-port-linuxkpi-115commits` (original 115 commits before squashing)

### Commit History (recent)

```
c02154e447 linuxkpi: Move PROC_LOCK before tdfind
ad8f587d67 linuxkpi: Add rman_res_t for DragonFly
54905cc935 linuxkpi: Add PCI stubs and rename ida_init
468a0a3a2d linuxkpi: Add DragonFly VM and thread shims
6103239379 linuxkpi: Add DragonFly scheduler and VM helpers
84b6b732fc linuxkpi: Adjust domainset alloc shims
1efe33da8c linuxkpi: Relax callout_reset_sbt shim
339c179671 linuxkpi: Fix vm_fault_page_quick shim
892260b3f3 linuxkpi: Fix vm_page_flag_set macro
... (earlier import and DRM removal commits)
```

### Build Status

**Last tested:** 2026-01-31
**Result:** PASS (full `buildkernel` on X86_64_GENERIC)

LinuxKPI now builds cleanly in the DragonFly kernel. Remaining work is
functional validation (drm-kmod build and runtime) and reducing stubbed
compat glue where possible.

---

## Phase 0: DRM-Local Glue Removal - COMPLETED ✓

Phase 0 removed DragonFly's old DRM-local Linux-compat glue to prepare for the FreeBSD LinuxKPI import.

### Phase 0 Execution Summary

- [x] Remove `dev/drm/*` build glue from `sys/conf/files` (line span 2272-2590)
- [x] Remove DRM options from `sys/conf/options` (drop `DRM_DEBUG`/`VGA_SWITCHEROO`)
- [x] Remove DRM-local thread/proc hooks (`td_linux_task`, `p_linux_mm`, and drop callbacks)
- [x] Remove `share/man/man4/drm.4` to avoid stale userland guidance
- [x] Remove `drm.4` entry from `share/man/man4/Makefile`
- [x] Remove `sys/libkern/linux_idr.c` to avoid conflicts with FreeBSD LinuxKPI
- [x] Remove DRM Makefiles to prevent build recursion into DRM subtrees
- [x] Remove `drm` from `sys/dev/Makefile` subdir list
- [x] Remove DRM devices/options from `sys/config/LINT64`
- [x] Remove `sys/dev/drm/**` sources from the repository
- [x] Remove orphaned `sys/contrib/dev/drm/**` firmware files (227 Radeon GPU firmware binaries)

---

## Phase 0.A: Verification Build - COMPLETED ✓

After removing the DRM stack, verified that DragonFly builds correctly without stale references.

### Development Environment

**VM Access:**
```bash
ssh -p 6021 root@devbox.sector.int
```

**Source code location on VM:** `/usr/src`

**Syncing Changes to VM:**
```bash
# From local repo:
git push gitea port-linuxkpi

# On VM:
cd /usr/src
git fetch gitea port-linuxkpi
git reset --hard gitea/port-linuxkpi
```

**Build Commands:**
```bash
cd /usr/src
make -j$(sysctl -n hw.ncpu) buildworld
make -j$(sysctl -n hw.ncpu) buildkernel KERNCONF=X86_64_GENERIC
```

### Common Issues Fixed During Phase 0.A

| Issue | Cause | Fix |
|-------|-------|-----|
| `cd: /usr/src/sys/dev/drm/include/linux: No such file or directory` | include/Makefile tries to install DRM headers | Remove DRM header installation rules from include/Makefile |
| `fatal error: linux/slab.h: No such file or directory` (agp) | agp module includes DRM headers | Remove agp from kernel config (X86_64_GENERIC) |
| `fatal error: linux/fb.h: No such file or directory` (vga) | vga_switcheroo requires DRM headers | Remove video from DEV_SUPPORT in sys/platform/pc64/Makefile.inc |
| `fatal error: linux/types.h: No such file or directory` (apple_gmux) | apple_gmux requires DRM headers | Remove apple_gmux from build (clear sys/gnu/dev/misc/Makefile) |
| `cc1: error: /usr/src/sys/dev/drm/include: No such file or directory` | Kernel includes DRM paths | Remove INCLUDES from sys/conf/kern.pre.mk |
| `fatal error: drm/drm.h: No such file or directory` (kdump) | kdump includes DRM headers for ioctl decoding | Remove DRM includes from usr.bin/kdump/Makefile and mkioctls |
| `fatal error: drm/drm.h: No such file or directory` (truss) | truss includes DRM headers | Remove DRM include from usr.bin/truss/Makefile |
| `fatal error: uapi_drm/drm.h: No such file or directory` (testvblank) | testvblank test requires DRM | Remove testvblank from test/debug/Makefile |

---

## Phase 1: FreeBSD LinuxKPI Import

### Phase 1A: Core Foundation - COMPLETED ✓

**Goal:** Port Concurrency Kit (CK) for RCU support

**Completed Tasks:**
1. ✓ Created vendor/CK branch with upstream CK 0.7.0 (commit 2265c784)
2. ✓ Applied FreeBSD exclusions (doc/, regressions/, tools/, etc.)
3. ✓ Merged vendor/CK to port-linuxkpi branch
4. ✓ Added DragonFly-specific modifications:
   - `sys/contrib/ck/include/ck_md.h` - Machine-dependent configuration
   - Modified standard headers for DragonFly kernel support
   - Fixed `ck_hp.c` to use DragonFly's `kqsort()`/`kbsearch()`
5. ✓ Added build integration to `sys/conf/files` and `sys/conf/kern.pre.mk`
6. ✓ Added README.DRAGONFLY and README.DELETED

**Verification:**
- ✓ Kernel builds successfully with CK
- ✓ CK symbols present in kernel (`_ck_epoch_addref`, `_ck_epoch_delref`)
- ✓ No build errors

---

### Phase 1B: Import LinuxKPI Headers and Core Implementation - COMPLETED ✓

**Goal:** Import FreeBSD LinuxKPI headers and core implementation files

**Completed Tasks:**
1. ✓ Created directory structure `sys/compat/linuxkpi/`
2. ✓ Imported headers from `common/include/` (linux/, asm/, video/, etc.)
3. ✓ Imported dummy headers from `dummy/include/`
4. ✓ Imported implementation files from `common/src/`
5. ✓ Added DragonFly-specific adaptations (__DragonFly__ checks)
6. ✓ Created README.DRAGONFLY documenting the import
7. ✓ Added build integration to `sys/conf/kmod.mk` and `sys/conf/kern.pre.mk`

**Source:** FreeBSD commit 79b05e7f80eb482287c700f10da9084824199a05

---

### Phase 1C: Build Integration and Compilation Fixes - COMPLETED ✓

**Goal:** Add LinuxKPI source files to kernel build and fix compilation errors

**Completed Tasks:**
1. ✓ Added all LinuxKPI source files to `sys/conf/files`
2. ✓ Configured include paths in `sys/conf/kern.pre.mk` and `sys/conf/kmod.mk`
3. ✓ Fixed initial compilation errors (basic type mismatches, missing includes)
4. ✓ Removed non-GPU code (Phase 2B) to simplify build

**Remaining work moved to Phase 2C** - API-level fixes for FreeBSD/DragonFly differences.

---

## Phase 2: Fix Compilation Errors in LinuxKPI Source Files

**Status:** In Progress (Phase 2C active)
**Goal:** Fix remaining LinuxKPI source files that fail to compile

### Phase 2A: Core File Compilation Fixes - COMPLETED ✓

Four core files were identified and fixed:

1. **linux_fpu.c** - FPU context switching code ✓
2. **linux_eventfd.c** - Eventfd file descriptor handling ✓
3. **linux_folio.c** - Memory/page allocation code ✓
4. **linux_current.c** - Current task/thread management ✓

#### Implementation Decisions

1. **Thread struct modifications**: Modify DragonFly's core `struct thread`
   - Add `td_lkpi_task`, `td_pflags`, `td_name`, `td_tid` fields
   - Cleaner approach than auxiliary storage

2. **Memory allocator**: Track allocation type internally
   - Implement type tracking within LinuxKPI wrappers
   - Maintain clean separation between FreeBSD-style and DragonFly APIs

3. **Approach**: Batch-by-batch analysis
   - Fix one file at a time
   - Analyze actual source code before implementing fixes
   - Test after each batch

---

### Phase 2B: Remove Non-GPU LinuxKPI Code

**Status:** COMPLETED ✓
**Completed:** 2026-01-31
**Goal:** Remove all networking, wireless, USB, and modem-related LinuxKPI code to focus exclusively on DRM/GPU support.

#### Rationale

Analysis of the drm-kmod repository (`~/s/drm-kmod/`) confirms:
- **Zero dependencies** on networking headers (skbuff, netdevice, cfg80211, mac80211)
- **Zero dependencies** on USB headers
- **Zero dependencies** on MHI (modem) headers
- drm-kmod requires only GPU/graphics-related LinuxKPI functionality

Removing non-GPU code reduces:
- Build time and complexity
- Maintenance burden
- Potential for conflicts with DragonFly's native networking stack

#### Source Files to DELETE (6 files, ~12,530 lines)

| File | Lines | Purpose |
|------|-------|---------|
| `sys/compat/linuxkpi/common/src/linux_80211.c` | 9,145 | WiFi 802.11 support |
| `sys/compat/linuxkpi/common/src/linux_80211_macops.c` | 759 | WiFi MAC operations |
| `sys/compat/linuxkpi/common/src/linux_skbuff.c` | 361 | Socket buffers (networking) |
| `sys/compat/linuxkpi/common/src/linux_netdev.c` | 445 | Network device layer |
| `sys/compat/linuxkpi/common/src/linux_mhi.c` | 89 | Modem Host Interface |
| `sys/compat/linuxkpi/common/src/linux_usb.c` | 1,731 | USB stack |

#### Header Directories to DELETE (2 directories)

| Directory | Contents | Purpose |
|-----------|----------|---------|
| `sys/compat/linuxkpi/common/include/net/` | 14+ files (~156KB) | WiFi/networking headers including cfg80211.h (61KB), mac80211.h (77KB) |
| `sys/compat/linuxkpi/dummy/include/net/` | 3 files | Empty networking stubs (gso.h, rtnetlink.h, page_pool/) |

#### Header Files to DELETE from `linux/` (10 files, ~70KB)

| File | Size | Purpose |
|------|------|---------|
| `linux/mhi.h` | 5.6KB | Modem Host Interface |
| `linux/netdevice.h` | 13.4KB | Network device definitions |
| `linux/skbuff.h` | 25.6KB | Socket buffer definitions |
| `linux/etherdevice.h` | 4.0KB | Ethernet device helpers |
| `linux/if_ether.h` | 3.0KB | Ethernet protocol definitions |
| `linux/inetdevice.h` | 150B | IP device definitions |
| `linux/ip.h` | 2.4KB | IP header definitions |
| `linux/tcp.h` | 2.3KB | TCP header definitions |
| `linux/udp.h` | 1.9KB | UDP header definitions |
| `linux/usb.h` | 11.6KB | USB device definitions |

#### Kernel Config to DELETE

| File | Reason |
|------|--------|
| `sys/config/X86_64_DRM` | No longer needed; use X86_64_GENERIC for all builds |

#### Build Configuration Updates (`sys/conf/files`)

**Remove these 6 entries:**

```
# Line 62-66: WiFi support (remove entirely)
# linux_80211*.c are only needed for WiFi drivers, not DRM/GPU
compat/linuxkpi/common/src/linux_80211.c        optional wlan \
    compile-with "${LINUXKPI_C}"
compat/linuxkpi/common/src/linux_80211_macops.c optional wlan \
    compile-with "${LINUXKPI_C}"

# Line 111-112: Modem support (remove entirely)
compat/linuxkpi/common/src/linux_mhi.c          standard \
    compile-with "${LINUXKPI_C}"

# Line 113-114: Network device support (remove entirely)
compat/linuxkpi/common/src/linux_netdev.c       standard \
    compile-with "${LINUXKPI_C}"

# Line 135-136: Socket buffer support (remove entirely)
compat/linuxkpi/common/src/linux_skbuff.c       standard \
    compile-with "${LINUXKPI_C}"

# Line 141-143: USB support (remove entirely)
# linux_usb.c requires USB subsystem - make optional for DRM-only builds
compat/linuxkpi/common/src/linux_usb.c          optional usb \
    compile-with "${LINUXKPI_C}"
```

#### Execution Steps

```bash
# Step 1: Delete source files (6 files)
rm sys/compat/linuxkpi/common/src/linux_80211.c
rm sys/compat/linuxkpi/common/src/linux_80211_macops.c
rm sys/compat/linuxkpi/common/src/linux_skbuff.c
rm sys/compat/linuxkpi/common/src/linux_netdev.c
rm sys/compat/linuxkpi/common/src/linux_mhi.c
rm sys/compat/linuxkpi/common/src/linux_usb.c

# Step 2: Delete header directories
rm -r sys/compat/linuxkpi/common/include/net/
rm -r sys/compat/linuxkpi/dummy/include/net/

# Step 3: Delete networking/USB headers from linux/
rm sys/compat/linuxkpi/common/include/linux/mhi.h
rm sys/compat/linuxkpi/common/include/linux/netdevice.h
rm sys/compat/linuxkpi/common/include/linux/skbuff.h
rm sys/compat/linuxkpi/common/include/linux/etherdevice.h
rm sys/compat/linuxkpi/common/include/linux/if_ether.h
rm sys/compat/linuxkpi/common/include/linux/inetdevice.h
rm sys/compat/linuxkpi/common/include/linux/ip.h
rm sys/compat/linuxkpi/common/include/linux/tcp.h
rm sys/compat/linuxkpi/common/include/linux/udp.h
rm sys/compat/linuxkpi/common/include/linux/usb.h

# Step 4: Delete kernel config
rm sys/config/X86_64_DRM

# Step 5: Update sys/conf/files
# - Remove 6 file entries listed above
# - Remove associated comments

# Step 6: Commit
git add -A
git commit -m "linuxkpi: Remove non-GPU code (networking, wireless, USB, modem)

Remove all LinuxKPI code not required for DRM/GPU support:
- linux_80211.c, linux_80211_macops.c (WiFi 802.11)
- linux_skbuff.c (socket buffers)
- linux_netdev.c (network devices)
- linux_mhi.c (modem host interface)
- linux_usb.c (USB stack)

Also remove associated headers:
- Entire net/ directory (cfg80211, mac80211, etc.)
- linux/netdevice.h, linux/skbuff.h, linux/usb.h, etc.

Delete X86_64_DRM kernel config (use X86_64_GENERIC).

Analysis of drm-kmod confirms zero dependencies on removed code.
This reduces ~12,500 lines of source and ~230KB of headers."

# Step 7: Push and test
git push gitea port-linuxkpi
# Run build test with X86_64_GENERIC
```

#### Summary Statistics

| Category | Count | Size |
|----------|-------|------|
| Source files deleted | 6 | ~12,530 lines |
| Header directories deleted | 2 | ~160KB |
| Header files deleted | 10 | ~70KB |
| Kernel configs deleted | 1 | ~12KB |
| conf/files entries removed | 6 | - |
| **Total code removed** | - | **~12,530 lines + ~230KB headers** |

#### Outcome

Phase 2B completed successfully:
- ✓ All 6 networking/USB/modem source files deleted
- ✓ Both `net/` header directories removed
- ✓ All 10 networking headers from `linux/` removed
- ✓ `X86_64_DRM` kernel config deleted (use `X86_64_GENERIC`)
- ✓ `sys/conf/files` updated to remove 6 file entries
- ✓ LinuxKPI is now GPU/DRM-focused only

---

### Phase 2C: Fix Remaining Compilation Errors

**Status:** BUILD CLEAN ✓ (functional validation pending)
**Started:** 2026-01-31
**Goal:** Fix API mismatches between FreeBSD and DragonFly in LinuxKPI source files

#### What was fixed

- PCI MSI inline clash and PCI helper gaps (MSI/MSI-X shims and fallbacks)
- I2C bus interface naming differences (`iicbus_*` method mapping)
- Netdevice notifier stubs for DRM-only build
- Memory/page API mismatches (`vm_page_initfake`, `jiffies`, vm fault helpers)
- Scheduler/thread helpers (PROC_LOCK ordering, tdfind/tdsignal shims)
- Kernel callout and timer shims (hrtimer path)
- IDR/IDA symbol conflict (`ida_init` rename)

#### Current status

- `quickkernel` and full `buildkernel` pass on X86_64_GENERIC
- Remaining work is drm-kmod build and runtime validation, and replacing
  stubbed PCI helpers with real DragonFly implementations where needed

---

### Phase 2.1: Eventfd Port (Required for drm-kmod)

**Status:** COMPLETED ✓
**Completed:** 2026-01-30
**Goal:** Provide kernel eventfd support sufficient for drm-kmod syncobj eventfd IOCTLs.

**Why needed:** drm-kmod uses eventfd in `drivers/gpu/drm/drm_syncobj.c` for `DRM_IOCTL_SYNCOBJ_EVENTFD`.

**Minimum API surface (for drm-kmod):**
- `eventfd_ctx_fdget(int fd)`
- `eventfd_ctx_put(struct eventfd_ctx *ctx)`
- `eventfd_signal(struct eventfd_ctx *ctx)`

**Status update (2026-01-30): Steps 1–2 complete**
- Added FreeBSD-style `struct selinfo` + select support (selrecord/selwakeup/seldrain/seltdfini) and wired `td_sel` in `struct thread`
- Ported `struct knlist`, `knlist_*`, and `KNOTE_LOCKED` into DragonFly headers and `sys/kern/kern_event.c`, including `knlist_init_mtx()`
- Decision: keep DragonFly's minimal `struct fileops`, add only `fo_flags`
- Trimmed `linuxfileops` to match the minimal DragonFly fileops layout and retained `fo_flags = DFLAG_PASSABLE`
- Added kernel eventfd core (`sys/kern/sys_eventfd.c`), kernel header (`sys/sys/eventfd.h`), and `DTYPE_EVENTFD` in `sys/sys/file.h`
- Wired LinuxKPI eventfd bridge to the kernel API; removed stubbed eventfd definitions from `sys/compat/linuxkpi/common/include/sys/eventfd.h`

---

## DRM-KMOD Requirements

### Overview

The drm-kmod repository contains:
- **Core DRM**: Modern Linux DRM stack (GEM, atomic, sync objects)
- **Intel i915**: Intel graphics driver (x86/x86_64 only)
- **AMD GPU**: amdgpu driver (amd64, aarch64, powerpc64, riscv64)
- **Radeon**: Legacy AMD driver
- **TTM**: GPU memory manager
- **DMA-BUF**: Buffer sharing infrastructure

### Linux Headers Required by DRM-KMOD

drm-kmod requires **199 unique Linux headers**. Key categories:

**Core Foundation:**
- `linux/compiler_types.h`, `linux/types.h`, `linux/kernel.h`
- `linux/module.h`, `linux/moduleparam.h`, `linux/init.h`
- `linux/export.h`, `linux/errno.h`, `linux/err.h`

**Data Structures:**
- `linux/list.h`, `linux/llist.h`, `linux/rbtree.h`
- `linux/idr.h`, `linux/xarray.h`, `linux/radix-tree.h`
- `linux/kref.h`, `linux/refcount.h`

**Memory Management:**
- `linux/mm.h`, `linux/slab.h`, `linux/gfp.h`, `linux/vmalloc.h`
- `linux/dma-mapping.h`, `linux/dma-buf.h`, `linux/scatterlist.h`
- `linux/shmem_fs.h`, `linux/mempool.h`

**Synchronization:**
- `linux/spinlock.h`, `linux/mutex.h`, `linux/rwsem.h`, `linux/ww_mutex.h`
- `linux/wait.h`, `linux/completion.h`, `linux/barrier.h`
- `linux/atomic.h`, `linux/rcupdate.h`, `linux/srcu.h`

**Device Model:**
- `linux/device.h`, `linux/kobject.h`, `linux/sysfs.h`
- `linux/pci.h`, `linux/platform_device.h`, `linux/acpi.h`
- `linux/dmi.h`, `linux/firmware.h`

**Graphics-Specific:**
- `linux/dma-fence.h`, `linux/dma-resv.h`, `linux/sync_file.h`
- `linux/fb.h`, `linux/backlight.h`, `linux/aperture.h`
- `linux/hdmi.h`, `linux/cec.h`, `linux/vgaarb.h`
- `linux/eventfd.h` (for syncobj)

### LinuxKPI Source Files Required for DRM

**Core (Non-Negotiable):**
| File | Purpose | Priority |
|------|---------|----------|
| `linux_compat.c` | Basic compatibility | CRITICAL |
| `linux_slab.c` | Memory allocation | CRITICAL |
| `linux_idr.c` | ID allocation | CRITICAL |
| `linux_xarray.c` | XArray (modern IDR) | CRITICAL |
| `linux_radix.c` | Radix tree (legacy) | HIGH |
| `linux_lock.c` | Locking primitives | CRITICAL |
| `linux_rcu.c` | RCU (needs ck_epoch) | CRITICAL |
| `linux_work.c` | Workqueues | CRITICAL |
| `linux_kobject.c` | Device model | CRITICAL |

**Memory & DMA (Critical for GPU):**
| File | Purpose | Priority |
|------|---------|----------|
| `linux_page.c` | Page operations | HIGH |
| `linux_shmemfs.c` | Shared memory | HIGH |
| `linux_dma_buf.c` | DMA buffer sharing | CRITICAL |
| `linux_fence.c` | DMA fences | CRITICAL |

**PCI & Bus Support:**
| File | Purpose | Priority |
|------|---------|----------|
| `linux_pci.c` | PCI subsystem | CRITICAL |
| `linux_i2c.c` | I2C bus | HIGH |
| `linux_i2cbb.c` | I2C bit-banging | MEDIUM |

**Graphics-Specific:**
| File | Purpose | Priority |
|------|---------|----------|
| `linux_aperture.c` | Graphics aperture | HIGH |
| `linux_hdmi.c` | HDMI/CEC | MEDIUM |
| `linux_eventfd.c` | Eventfd (syncobj) | HIGH |

**Timer & IRQ:**
| File | Purpose | Priority |
|------|---------|----------|
| `linux_hrtimer.c` | High-res timers | HIGH |
| `linux_interrupt.c` | IRQ handling | HIGH |
| `linux_tasklet.c` | Tasklets | HIGH |

**Power Management & Support:**
| File | Purpose | Priority |
|------|---------|----------|
| `linux_firmware.c` | Firmware loading | HIGH |
| `linux_dmi.c` | DMI/SMBIOS | MEDIUM |
| `linux_acpi.c` | ACPI integration | MEDIUM |

---

## DragonFly-Specific Challenges for DRM

### 1. Concurrency Kit (ck_epoch) - COMPLETED ✓

**Status:** Ported successfully. DRM RCU support is functional.

### 2. DMA-BUF / DMA-Fence - CRITICAL

**Status:** Requires porting from FreeBSD LinuxKPI.

**Why it matters:**
- Core DRM functionality for buffer sharing between drivers
- Required for PRIME (GPU buffer export/import)
- Required for dma-buf sync file support

### 3. Backlight Subsystem - HIGH

**Status:** DragonFly has no backlight control subsystem.

**Why it matters:**
- Required for laptop display brightness control
- i915 and amdgpu both need this

**Decision:** Port FreeBSD's `sys/dev/backlight/` subsystem.

### 4. Aperture / Graphics Memory - HIGH

**Status:** Needs porting via `linux_aperture.c`.

**Why it matters:**
- Required for legacy graphics memory access
- Some Intel and AMD chips need this

### 5. Floating Point in Kernel - MEDIUM

**Status:** Needs investigation.

**Why it matters:**
- AMD Display Core (DC) requires FP support
- DC is 1000+ files of display code

---

## Success Metrics

1. **Build Success:** LinuxKPI compiles without errors
2. **DRM-KMOD Build:** External drm-kmod repo can build against DragonFly
3. **Basic GPU Detection:** Can probe Intel/AMD GPUs
4. **Display Output:** Framebuffer console or X11 works on at least one GPU

---

## Risk Assessment

| Risk | Impact | Mitigation |
|------|--------|------------|
| DMA-BUF implementation | HIGH | Study FreeBSD implementation carefully |
| FP in kernel for AMD DC | MEDIUM | Test early; may need DC workarounds |
| DragonFly PCI differences | MEDIUM | Adapt `linux_pci.c` as needed |
| Thread/scheduler differences | MEDIUM | Test RCU integration thoroughly |

---

## DRM-KMOD Validation Plan (QEMU-first)

### QEMU-first loop (low-turnover)

1. **Prepare sources**
   - Record DragonFly commit hash used for the kernel build.
   - Record drm-kmod commit hash.
   - Repo locations:
     - VM: `/opt/s/drm-kmod`
     - Local: `~/s/drm-kmod`
2. **Build drm-kmod (clean)**
   - Build against the DragonFly source tree for the recorded commit.
   - Capture missing headers/symbols and API mismatches.
3. **Module load sanity (no GPU required)**
   - Load drm-kmod modules in QEMU without passthrough.
   - Verify clean load/unload and no unresolved symbols.
4. **Fix build/runtime errors (iterative)**
   - Add minimal LinuxKPI shims or DragonFly mappings only when exercised.
5. **Gate for hardware**
   - Only move to real hardware once drm-kmod builds cleanly and modules load cleanly in QEMU.

### Hardware milestone (when ready)

1. **Install kernel + modules**
   - Install updated kernel and drm-kmod modules.
2. **Boot and probe**
   - Load required DRM modules and confirm GPU probe.
3. **Functional smoke test**
   - Validate console/X11/Wayland and basic GPU activity.

---

## Phase 3: LinuxKPI Stub Implementation (CRITICAL FOR GPU/DRM)

### Overview

Analysis of the DragonFly LinuxKPI compatibility layer has revealed **150+ stub implementations** that break functionality required for GPU/DRM support. These stubs were created to allow compilation but cause runtime failures, crashes, or silent data corruption.

**Key Finding:** FreeBSD's LinuxKPI implementation is **fundamentally correct** and works for drm-kmod. DragonFly's port has broken compatibility stubs in `dragonfly_compat.h` and related files that must be replaced with proper implementations following FreeBSD's approach.

### Critical Stub Analysis

#### CRITICAL Priority (System Crashes or DRM Non-Functional)

| # | Stub | Location | Issue | Impact |
|---|------|----------|-------|--------|
| 1 | `taskqueue_drain_all()` | `dragonfly_compat.h:742-754` | **No-op stub**: Only calls `taskqueue_block/unblock()`, does NOT drain tasks | Workqueue flush/drain operations don't wait for completion, causing race conditions and use-after-free when work continues after structures are freed |
| 2 | `taskqueue_poll_is_busy()` | `dragonfly_compat.h:622-630` | **Returns 0**: Always reports "not busy" | `flush_work()`, `work_busy()` return incorrect results, work items appear complete when still executing |
| 3 | `linuxdev_ops` | `linux_compat.c:1521-1531` | **All ops = NULL**: Device operations structure has NULL function pointers | DRM device open/close/ioctl/mmap operations will fail; DRM drivers cannot access hardware |
| 4 | `GFP_NOWAIT` handler | `linux_shmemfs.c:51` | **Calls panic()**: Unimplemented GFP flag causes system crash | System will crash if any driver uses `GFP_NOWAIT` flag |
| 5 | `vm_radix_*` (10+ funcs) | `vm/vm_radix.h:49-136` | **All return 0/NULL**: Radix tree operations are stubs | GPU page tracking via radix tree completely broken; pages can be lost or corrupted |
| 6 | `sf_buf_kva()` | `sys/sf_buf.h:108-112` | **Returns 0**: No kernel virtual address mapping | DMA operations using sendfile buffers access wrong memory address; data corruption |
| 7 | `vm_reserv_*` (10+ funcs) | `vm/vm_reserv.h:46-123` | **Return ENOMEM or 0**: VM reservation stubs | GPU memory reservations fail silently or report failure; memory allocation unreliable |

**Current Impact:** The `taskqueue_drain_all()` and `taskqueue_poll_is_busy()` stubs are **already causing test failures** in the workqueue test suite. Work items are not properly waited for, causing race conditions and premature test completion.

#### HIGH Priority (Major Functional Failures)

| # | Stub | Location | Issue | Impact |
|---|------|----------|-------|--------|
| 8 | `cap_rights_limit()` | `sys/capsicum.h:197-202` | **Returns 0**: Capability restrictions not enforced | **Security**: Sandboxing bypassed; not critical for GPU but affects system security |
| 9 | `cap_enter()` | `sys/capsicum.h:205-210` | **No-op**: Capability mode not entered | **Security**: Same as above |
| 10 | `uma_zcreate/alloc/free()` | `vm/uma.h:109-152` | **Fallback to malloc/free**: No UMA zone caching | Performance degradation; UMA benefits (CPU cache affinity, reduced fragmentation) lost |
| 11 | `pcpu_alloc/malloc()` | `sys/pcpu.h:102-128` | **Returns NULL**: Per-CPU data allocation fails | Per-CPU counters and data structures cannot be allocated; performance impact |
| 12 | `vga_get/put()` | `linux/vgaarb.h:107-167` | **Return 0/no-op**: VGA arbitration stubs | Multi-GPU systems may conflict over VGA resources; single GPU unaffected |
| 13 | `stack_save()` | `sys/stack.h:51-56` | **Sets depth=0**: Stack traces empty | Debugging impossible when stack traces show no frames |
| 14 | `pci_iov_init_t` | `dragonfly_compat.h:2067-2069` | **Type only**: SR-IOV (GPU virtualization) not supported | GPU virtualization for VMs won't work; passthrough unaffected |
| 15 | `pctrie_*` | `dragonfly_compat.h:2108-2167` | **Stubs**: PCIe DMA tracking disabled | DMA operations lack tracking; may work but without verification |

### Root Cause Comparison: FreeBSD vs DragonFly

#### Taskqueue Drain Implementation

**FreeBSD** (`/sys/kern/subr_taskqueue.c:623-634`):
```c
void
taskqueue_drain_all(struct taskqueue *queue)
{
    if (!queue->tq_spin)
        WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL, __func__);

    TQ_LOCK(queue);
    (void)taskqueue_drain_tq_queue(queue);   // Drains pending queue
    (void)taskqueue_drain_tq_active(queue);  // Drains active tasks
    TQ_UNLOCK(queue);
}
```

**DragonFly** (`dragonfly_compat.h:742-754`):
```c
static __inline void
taskqueue_drain_all(struct taskqueue *tq)
{
    /* 
     * DragonFly doesn't have taskqueue_drain_all.
     * This is a no-op stub - callers should use taskqueue_drain()
     * for specific tasks, or taskqueue_free() will drain on destroy.
     */
    taskqueue_block(tq);
    taskqueue_unblock(tq);
}
```

**Analysis:** FreeBSD's implementation properly drains both the pending task queue AND active (executing) tasks. DragonFly's stub only blocks/unblocks the taskqueue without draining anything.

#### Taskqueue Poll Is Busy Implementation

**FreeBSD** (`/sys/kern/subr_taskqueue.c:541-551`):
```c
int
taskqueue_poll_is_busy(struct taskqueue *queue, struct task *task)
{
    int retval;

    TQ_LOCK(queue);
    retval = task->ta_pending > 0 || task_get_busy(queue, task) != NULL;
    TQ_UNLOCK(queue);

    return (retval);
}
```

**DragonFly** (`dragonfly_compat.h:618-631`):
```c
static __inline int
taskqueue_poll_is_busy(struct taskqueue *tq __unused, struct task *t __unused)
{
    /* DragonFly doesn't have this API - return "not busy" */
    return (0);
}
```

**Analysis:** FreeBSD checks if task is pending in queue OR currently executing. DragonFly always returns "not busy", causing incorrect state reporting.

### Implementation Plan (Following FreeBSD)

#### Phase 3A: Fix CRITICAL Taskqueue Stubs (HIGHEST PRIORITY)

**Status:** COMPLETED ✓ (2026-02-01)

**Goal:** Implement proper `taskqueue_drain_all()` and `taskqueue_poll_is_busy()` for DragonFly.

**Approach:**
1. ✓ **Study FreeBSD implementation** in `/sys/kern/subr_taskqueue.c`
2. ✓ **Implement in DragonFly kernel** (`sys/kern/subr_taskqueue.c`):
   - Added `taskqueue_drain_all()` that drains both queue and active tasks
   - Added `taskqueue_poll_is_busy()` that checks pending and active status
3. ✓ **Remove stubs** from `dragonfly_compat.h`
4. ✓ **Test** with kernel build (successful - 18 minutes, no errors)

**Key Implementation Details from FreeBSD:**
- `taskqueue_drain_tq_queue()`: Drains tasks from pending queue
- `taskqueue_drain_tq_active()`: Waits for and drains currently executing tasks
- `task_get_busy()`: Checks if task is currently executing on a thread

**Files to Modify:**
- `sys/kern/subr_taskqueue.c` - Add real implementations
- `sys/sys/taskqueue.h` - Add function declarations
- `sys/compat/linuxkpi/common/include/linux/dragonfly_compat.h` - Remove stubs

**Success Criteria:**
- ✓ Kernel builds successfully with new implementations
- ✓ No compilation errors or warnings
- ✓ Build time: ~18 minutes (quickkernel)
- ⏳ Run workqueue regression suite (tests 1–44) via `dfregress -r linuxkpi_workqueue.run`
- ⏳ `flush_workqueue()` and `drain_workqueue()` properly wait for completion (validated via tests)
- ⏳ No race conditions in work re-queuing scenarios (validated via tests)

**Completed Work:**
- `taskqueue_drain_all()` added to `sys/kern/subr_taskqueue.c` (lines 522-554)
- `taskqueue_poll_is_busy()` added to `sys/kern/subr_taskqueue.c` (lines 559-569)
- Function declarations added to `sys/sys/taskqueue.h` (lines 92, 96)
- Stubs removed from `dragonfly_compat.h` (lines 742-754, 622-631)
- Kernel build validated: commit `ef2ac85ff6` builds successfully
- Workqueue test suite implemented in `test/testcases/linuxkpi/workqueue/` and `test/testcases/linuxkpi_workqueue.run`

#### Phase 3B: Fix Device Operations (DRM Critical)

**Goal:** Implement proper `linuxdev_ops` for DRM device access.

**Approach (Detailed Plan):**
1. **Baseline review and mapping**
   - Compare FreeBSD LinuxKPI device ops in `sys/compat/linuxkpi/common/src/linux_compat.c` to DragonFly's dev_ops model in `sys/sys/device.h`.
   - Identify how DragonFly drivers wire `struct dev_ops` (open/read/write/ioctl/mmap/kqfilter) in examples like `sys/net/netmap/netmap_freebsd.c` and `sys/net/tun/if_tun.c`.
   - Confirm how LinuxKPI uses `linux_dev_fdopen()` and `linuxfileops` to bridge into `struct linux_file`.

2. **Implement DragonFly dev_ops wrappers (linuxdev_ops)**
   - Add wrappers that funnel into the existing LinuxKPI file layer in `sys/compat/linuxkpi/common/src/linux_compat.c`:
     - `d_open`: create a `struct file` and call `linux_dev_fdopen()` to attach a `struct linux_file` and install `linuxfileops`.
     - `d_close`: call `linux_file_close()`.
     - `d_read`: call `linux_file_read()`.
     - `d_write`: call `linux_file_write()`.
     - `d_ioctl`: call `linux_file_ioctl()`.
     - `d_mmap`: call `linux_file_mmap()`.
     - `d_mmap_single`: call `linux_file_mmap_single()` (or the same path as mmap) as supported by DragonFly.
     - `d_kqfilter`: call `linux_file_kqfilter()`.
   - Ensure each wrapper pulls the correct `struct file *` and `cdev_t` from `struct dev_*_args` and uses the same error translation patterns already in `linux_file_*`.
   - Keep `D_MPSAFE` in `linuxdev_ops.head.flags`.

3. **Fix LinuxKPI cdev creation on DragonFly**
   - Implement the DragonFly branches of `cdev_add()` and `cdev_add_ext()` in `sys/compat/linuxkpi/common/include/linux/cdev.h`:
     - Use `make_dev()` / `make_only_dev()` to create devfs nodes with `linuxdev_ops`.
     - Set `si_drv1` to the `struct linux_cdev *` so `linux_dev_fdopen()` can find it.
     - Preserve the kobject name (`kobject_name(&cdev->kobj)`) and apply uid/gid/mode in `_ext`.
     - Store the created `cdev_t` in `ldev->cdev` for later cleanup.
   - Confirm `linux_destroy_dev()` and `linux_cdev_release()` still call `destroy_dev()` correctly.

4. **Wire device model details**
   - Update `linux_iminor()` DragonFly branch if needed to ensure it recognizes devices created via `linuxdev_ops` and `si_drv1`.
   - Ensure any devfs permissions or naming requirements used by drm-kmod are respected (e.g., `card0`, `renderD*`).

5. **Build and runtime validation**
   - Build the kernel with the new `linuxdev_ops`.
   - Load a LinuxKPI consumer module that registers a cdev (drm-kmod when ready).
   - Verify device node creation and basic open/ioctl paths:
     - `ls -la /dev/drm*`
     - `kldload /path/to/drm.ko`
   - If needed, add a minimal test module to exercise open/close/ioctl without hardware.

**Files to Modify:**
- `sys/compat/linuxkpi/common/src/linux_compat.c` - Implement device ops
- `sys/compat/linuxkpi/common/include/linux/cdev.h` - Implement DragonFly cdev creation

**Success Criteria:**
- DRM device nodes can be opened/closed
- DRM ioctls work correctly
- GPU device can be accessed by userspace

#### Phase 3C: Fix VM and Memory Stubs (HIGH PRIORITY)

**Goal:** Implement `vm_radix_*`, `vm_reserv_*`, and `sf_buf_kva()` properly.

**Approach (Detailed Plan):**
1. **sf_buf_* (must-fix before DRM runtime)**
   - Current LinuxKPI stub in `sys/compat/linuxkpi/common/include/sys/sf_buf.h`
     returns `0` for `sf_buf_kva()` and uses dummy alloc/free.
   - LinuxKPI paths that rely on it:
     - `sys/compat/linuxkpi/common/include/linux/highmem.h`
     - `sys/compat/linuxkpi/common/include/linux/scatterlist.h`
   - DragonFly already has a real sfbuf subsystem:
     - API: `sys/sys/sfbuf.h` (uses lwbuf)
     - Impl: `sys/kern/kern_sfbuf.c`
   - Plan:
     - Replace the LinuxKPI sf_buf stub with a compatibility bridge to
       DragonFly’s sfbuf (lwbuf) implementation.
     - Provide wrappers with the FreeBSD-style LinuxKPI signature
       (`sf_buf_alloc(vm_page_t, int flags)`), while routing to DragonFly
       `sf_buf_alloc(struct vm_page *)`.
     - Ensure `sf_buf_kva()` returns a valid kernel mapping and that
       `sf_buf_free()`/`sf_buf_ref()` map to DragonFly semantics.

2. **vm_radix_* (required for i915)**
   - Current LinuxKPI `sys/compat/linuxkpi/common/include/vm/vm_radix.h` is
     a stub that returns NULL/0 and fakes iteration.
   - drm-kmod i915 uses `vm_object_set_memattr()` and may iterate pages using
     `VM_RADIX_FORALL` or FreeBSD-style page iterators.
   - DragonFly’s `vm_page_next()` only walks consecutive pages, not a full
     tree traversal, so it cannot replace VM radix iteration by itself.
   - DragonFly resident pages are stored in an RB tree on the object:
     - `struct vm_object::rb_memq` in `sys/vm/vm_object.h`
     - RB tree helpers in `sys/vm/vm_page.h` / `sys/vm/vm_page.c`
   - Plan:
     - Implement a real radix-like container for LinuxKPI (RB-tree or
       existing DragonFly tree primitives), with correct lookup/insert/remove
       and iteration semantics.
     - Define and document locking expectations (caller holds object/lock).
     - Keep the API shape compatible with FreeBSD’s vm_radix use sites.

3. **vm_object_set_memattr() runtime behavior**
   - A minimal DragonFly wrapper was added for LinuxKPI to set `memattr` only
     when `resident_page_count == 0`.
   - i915 has a fallback path that iterates pages to update memattr when
     `vm_object_set_memattr()` fails; on DragonFly that iteration is currently
     not reliable.
   - Plan:
     - Prefer extending the wrapper to update existing resident pages’
       memattr using RB-tree iteration over `rb_memq`, so i915 does not need
       the fallback per-page loop.

4. **vm_reserv_***
   - Current LinuxKPI `sys/compat/linuxkpi/common/include/vm/vm_reserv.h` is
     stubbed (often returns ENOMEM or no-ops).
   - drm-kmod currently shows no direct `vm_reserv_*` use, but this must be
     confirmed during drm-kmod build and module load.
   - Plan:
     - If required, implement a minimal safe reservation layer, or map to
       DragonFly primitives where possible.
     - Avoid leaving stubs that silently succeed in safety-critical paths.

**Equivalence Notes (FreeBSD vs DragonFly):**
- `sf_buf_*`:
  - FreeBSD API in `~/s/freebsd/sys/sys/sf_buf.h` with implementation in
    `~/s/freebsd/sys/kern/subr_sfbuf.c`.
  - DragonFly API in `sys/sys/sfbuf.h` with implementation in
    `sys/kern/kern_sfbuf.c` using lwbuf + objcache.
  - Compat should wrap DragonFly’s real sfbuf (avoid LinuxKPI stub).
- `vm_radix_*`:
  - FreeBSD uses vm_radix/pctrie for VM object resident pages.
  - DragonFly uses RB trees (`rb_memq`) for resident pages.
  - Compat should use RB tree traversal for iteration and lookup semantics.
- `vm_object_set_memattr()`:
  - FreeBSD enforces “no resident pages” via `vm_radix_is_empty()`.
  - DragonFly has `memattr` field but no setter; compat wrapper must enforce
    safety and/or update existing pages as needed.
- `vm_reserv_*`:
  - FreeBSD provides a full superpage reservation subsystem.
  - DragonFly has no VM reservation subsystem in-tree.

**Alternative for vm_radix/vm_reserv:**
If DragonFly doesn't have equivalent functionality, DRM drivers might work with fallback implementations that use basic malloc/free with tracking.

**Files to Modify:**
- `sys/compat/linuxkpi/common/include/vm/vm_radix.h`
- `sys/compat/linuxkpi/common/include/vm/vm_reserv.h`
- `sys/compat/linuxkpi/common/include/sys/sf_buf.h`

#### Phase 3D: Fix Remaining HIGH Priority Stubs

**Goal:** Fix UMA, per-CPU, VGA arbitration, and stack tracing stubs.

**Priority Order:**
1. **UMA (`uma_zcreate/alloc/free`)** - Performance optimization (malloc fallback may be acceptable short-term)
2. **Per-CPU alloc (`pcpu_alloc/malloc`)** - Performance optimization (can use regular allocation for now)
3. **VGA arbitration (`vga_get/put`)** - Only needed for multi-GPU systems
4. **Stack tracing (`stack_save`)** - Debug feature only

**Approach:** Implement following FreeBSD patterns where applicable, or document that fallback behavior is acceptable for GPU/DRM use case.

**Detailed Notes:**
- UMA: prioritize correctness first, then perf; if malloc fallback is used,
  document expected overhead and add TODOs for zone caching.
- Per-CPU alloc: ensure non-NULL allocations for drm-kmod paths; perf tuning can follow.
- VGA arbitration: defer unless multi-GPU testing is planned.
- Stack save: keep as low-priority, but ensure panic/debug paths do not crash.

### Testing and Validation

#### Phase 3A Testing (Taskqueue)
```bash
# Build kernel with new taskqueue implementations
cd /usr/src
make -j$(sysctl -n hw.ncpu) buildkernel KERNCONF=X86_64_GENERIC

# Run workqueue tests
cd /usr/src/test/testcases
dfregress -r linuxkpi_workqueue.run

# Verify all tests pass:
# - Test 5: Multiple work items (5 items, max_active=4)
# - Test 7: Sustained work (100 items)
```

#### Phase 3B Testing (Device Ops)
```bash
# After drm-kmod build attempt
# Load DRM module and verify device node created
kldload /path/to/drm.ko
ls -la /dev/drm*
# Should show /dev/drm/card0 or similar
```

### Success Metrics

| Phase | Success Criteria |
|-------|------------------|
| 3A | Workqueue tests pass with 100+ items; no race conditions |
| 3B | DRM device node created; open/close/ioctl work |
| 3C | GPU memory allocation works; DMA operations succeed |
| 3D | Multi-GPU arbitration works (if tested) |

### Risk Assessment

| Risk | Impact | Mitigation |
|------|--------|------------|
| Taskqueue implementation complexity | MEDIUM | FreeBSD implementation is proven; follow closely |
| Device operations mapping differences | MEDIUM | DragonFly device model differs from FreeBSD; may need adaptation |
| VM subsystem differences | MEDIUM | DragonFly VM differs from FreeBSD; radix/reserv may need alternative approach |
| Performance degradation with stubs | LOW | Can use fallbacks initially; optimize later |

### File Inventory for Implementation

**Files requiring new implementations:**
1. `sys/kern/subr_taskqueue.c` - Add `taskqueue_drain_all()` and `taskqueue_poll_is_busy()`
2. `sys/sys/taskqueue.h` - Add declarations
3. `sys/compat/linuxkpi/common/src/linux_compat.c` - Fix `linuxdev_ops`
4. `sys/compat/linuxkpi/common/include/linux/dragonfly_compat.h` - Remove taskqueue stubs

**Files requiring investigation:**
5. `sys/compat/linuxkpi/common/include/vm/vm_radix.h` - Implement or provide fallback
6. `sys/compat/linuxkpi/common/include/vm/vm_reserv.h` - Implement or provide fallback
7. `sys/compat/linuxkpi/common/include/sys/sf_buf.h` - Fix `sf_buf_kva()`

**Files with acceptable stubs (for GPU use case):**
- `sys/compat/linuxkpi/common/include/sys/capsicum.h` - Security feature, not needed for GPU
- `sys/compat/linuxkpi/common/include/vm/uma.h` - Performance optimization only
- `sys/compat/linuxkpi/common/include/sys/pcpu.h` - Performance optimization only
- `sys/compat/linuxkpi/common/include/linux/vgaarb.h` - Only for multi-GPU
- `sys/compat/linuxkpi/common/include/sys/stack.h` - Debug feature only

---

## Current Implementation Status (as of 2026-02-01)

### Completed
- ✓ LinuxKPI core imported and building
- ✓ Non-GPU code removed (12,530+ lines)
- ✓ Basic taskqueue and workqueue infrastructure in place
- ✓ Eventfd support added (required for syncobj)

### In Progress
- ⏳ Device operations implementation (Phase 3B)

### Blocked/Pending
- ⏸ VM radix/reserv implementation (Phase 3C) - investigate if DRM actually needs these
- ⏸ DRM-KMOD build attempt - blocked until Phase 3B (device ops) complete

### Next Steps
1. ✓ ~~Phase 3A: Taskqueue drain implementation~~ (COMPLETED 2026-02-01)
2. **Phase 3B: Device operations implementation** - Implement `linuxdev_ops` in `linux_compat.c`
3. **Phase 3C: VM/Memory stubs** - Investigate if DRM actually needs `vm_radix_*` and `vm_reserv_*`
4. **DRM-KMOD build attempt** - Once device ops are functional
5. **Testing** - Load drm-kmod modules in QEMU, verify GPU detection

---

## Appendix A: Taskqueue Implementation Investigation (DragonFly vs FreeBSD)

**Investigation Date:** 2026-02-01
**Purpose:** Determine whether DragonFly needs FreeBSD's multi-task tracking mechanism for taskqueue

### A.1 Executive Summary

Both DragonFly and FreeBSD taskqueue implementations share a common origin (Doug Rabson's 2000 FreeBSD code), but have diverged significantly. **The most critical difference is in the "busy" tracking mechanism**, which is essential for proper task synchronization in multi-worker taskqueues.

**Key Finding:** DragonFly's native kernel code uses **only single-worker taskqueues**. The multi-worker requirement comes **exclusively from LinuxKPI workqueues** used by DRM/GPU drivers.

---

### A.2 Data Structure Comparison

#### A.2.1 DragonFly `struct taskqueue` (sys/kern/subr_taskqueue.c:49-63)

```c
struct taskqueue {
    STAILQ_ENTRY(taskqueue) tq_link;
    STAILQ_HEAD(, task)     tq_queue;
    const char              *tq_name;
    taskqueue_enqueue_fn    tq_enqueue;
    void                    *tq_context;

    struct task             *tq_running;    /* Single pointer - ONE task only */
    struct spinlock         tq_lock;
    struct thread           **tq_threads;
    int                     tq_tcount;
    int                     tq_flags;
    int                     tq_callouts;
};
```

#### A.2.2 FreeBSD `struct taskqueue` (sys/kern/subr_taskqueue.c:63-79)

```c
struct taskqueue {
    STAILQ_HEAD(, task)     tq_queue;
    LIST_HEAD(, taskqueue_busy) tq_active;  /* LIST of busy tasks - MULTIPLE */
    struct task             *tq_hint;       /* Optimization hint */
    u_int                   tq_seq;         /* Sequence number for ordering */
    int                     tq_callouts;
    struct mtx_padalign     tq_mutex;
    taskqueue_enqueue_fn    tq_enqueue;
    void                    *tq_context;
    char                    *tq_name;
    struct thread           **tq_threads;
    int                     tq_tcount;
    int                     tq_spin;
    int                     tq_flags;
    taskqueue_callback_fn   tq_callbacks[TASKQUEUE_NUM_CALLBACKS];
    void                    *tq_cb_contexts[TASKQUEUE_NUM_CALLBACKS];
};
```

#### A.2.3 FreeBSD `struct taskqueue_busy` (sys/kern/subr_taskqueue.c:56-61)

```c
struct taskqueue_busy {
    struct task             *tb_running;    /* Currently executing task */
    u_int                    tb_seq;        /* Sequence number when started */
    bool                     tb_canceling;  /* Task is being canceled */
    LIST_ENTRY(taskqueue_busy) tb_link;     /* Link in tq_active list */
};
```

**DragonFly does NOT have this structure.**

#### A.2.4 Critical Architectural Difference

| Aspect | DragonFly | FreeBSD |
|--------|-----------|---------|
| Running task tracking | Single `tq_running` pointer | `LIST_HEAD tq_active` of `taskqueue_busy` |
| Concurrent task support | **ONE task only** | **Multiple tasks** |
| Sequence numbers | Not present | `tq_seq` for drain ordering |
| Hint optimization | Not present | `tq_hint` for priority insertion |
| Cancel tracking | Not present | `tb_canceling` flag per task |

---

### A.3 API Function Comparison

#### A.3.1 Functions Present in Both

| Function | DragonFly | FreeBSD |
|----------|-----------|---------|
| `taskqueue_create()` | ✓ | ✓ |
| `taskqueue_free()` | ✓ | ✓ |
| `taskqueue_start_threads()` | ✓ | ✓ |
| `taskqueue_enqueue()` | ✓ | ✓ |
| `taskqueue_enqueue_flags()` | ✓ | ✓ |
| `taskqueue_enqueue_timeout()` | ✓ | ✓ |
| `taskqueue_cancel()` | ✓ | ✓ |
| `taskqueue_cancel_timeout()` | ✓ | ✓ |
| `taskqueue_drain()` | ✓ | ✓ |
| `taskqueue_drain_timeout()` | ✓ | ✓ |
| `taskqueue_drain_all()` | ✓ | ✓ |
| `taskqueue_poll_is_busy()` | ✓ | ✓ |
| `taskqueue_block()` | ✓ | ✓ |
| `taskqueue_unblock()` | ✓ | ✓ |
| `taskqueue_run()` | ✓ | ✓ |
| `taskqueue_member()` | ✗ | ✓ |

#### A.3.2 DragonFly-Only Extensions

| Function | Purpose |
|----------|---------|
| `taskqueue_find()` | Find taskqueue by name |
| `taskqueue_enqueue_optq()` | Allow task migration between queues |
| `taskqueue_cancel_simple()` | Cancel without knowing queue |
| `taskqueue_drain_simple()` | Drain without knowing queue |

#### A.3.3 FreeBSD-Only Features

| Function | Purpose |
|----------|---------|
| `taskqueue_create_fast()` | Create with spin mutex |
| `taskqueue_start_threads_cpuset()` | CPU affinity |
| `taskqueue_start_threads_in_proc()` | Specific process |
| `taskqueue_enqueue_timeout_sbt()` | sbintime precision |
| `taskqueue_quiesce()` | Repeated drain until truly empty |
| `taskqueue_set_callback()` | Thread init/shutdown callbacks |

---

### A.4 Implementation Comparison: Critical Functions

#### A.4.1 `taskqueue_drain_all()`

**FreeBSD Implementation** (sys/kern/subr_taskqueue.c:623-634):

```c
void
taskqueue_drain_all(struct taskqueue *queue)
{
    if (!queue->tq_spin)
        WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL, __func__);

    TQ_LOCK(queue);
    (void)taskqueue_drain_tq_queue(queue);   /* Step 1: Drain pending queue */
    (void)taskqueue_drain_tq_active(queue);  /* Step 2: Wait for active tasks */
    TQ_UNLOCK(queue);
}
```

**FreeBSD `taskqueue_drain_tq_queue()`** (lines 400-427):

```c
static int
taskqueue_drain_tq_queue(struct taskqueue *queue)
{
    struct task t_barrier;

    if (STAILQ_EMPTY(&queue->tq_queue))
        return (0);

    /*
     * Enqueue barrier task at TAIL with highest priority (UCHAR_MAX).
     * This prevents new tasks from jumping ahead while ensuring all
     * currently queued tasks execute first.
     */
    TASK_INIT(&t_barrier, UCHAR_MAX, taskqueue_task_nop_fn, &t_barrier);
    STAILQ_INSERT_TAIL(&queue->tq_queue, &t_barrier, ta_link);
    queue->tq_hint = &t_barrier;
    t_barrier.ta_pending = 1;

    /* Wait until barrier has been executed */
    while (t_barrier.ta_pending != 0)
        TQ_SLEEP(queue, &t_barrier, "tq_qdrain");
    return (1);
}
```

**FreeBSD `taskqueue_drain_tq_active()`** (lines 434-461):

```c
static int
taskqueue_drain_tq_active(struct taskqueue *queue)
{
    struct taskqueue_busy *tb;
    u_int seq;

    if (LIST_EMPTY(&queue->tq_active))
        return (0);

    /* Block taskq_terminate() during drain */
    queue->tq_callouts++;

    /* Capture current sequence number */
    seq = queue->tq_seq;

restart:
    /* Wait for any task with sequence <= our captured seq */
    LIST_FOREACH(tb, &queue->tq_active, tb_link) {
        if ((int)(tb->tb_seq - seq) <= 0) {
            TQ_SLEEP(queue, tb->tb_running, "tq_adrain");
            goto restart;  /* List may have changed, restart */
        }
    }

    /* Release taskqueue_terminate() */
    queue->tq_callouts--;
    if ((queue->tq_flags & TQ_FLAGS_ACTIVE) == 0)
        wakeup_one(queue->tq_threads);
    return (1);
}
```

**Current DragonFly Implementation** (sys/kern/subr_taskqueue.c:522-550):

```c
void
taskqueue_drain_all(struct taskqueue *queue)
{
    struct task *task;

    TQ_LOCK(queue);

    while ((task = STAILQ_FIRST(&queue->tq_queue)) != NULL ||
        queue->tq_running != NULL) {
        if (task != NULL) {
            TQ_SLEEP(queue, queue, "tqdall");
        } else {
            TQ_SLEEP(queue, queue, "tqdall");
        }
    }

    TQ_UNLOCK(queue);
}
```

**Analysis:**

| Aspect | FreeBSD | DragonFly (Current) |
|--------|---------|---------------------|
| Barrier task pattern | Yes - ensures ordering | No |
| Multi-task tracking | Yes - via `tq_active` list | No - single `tq_running` |
| Sequence numbers | Yes - ignores tasks started after drain began | No |
| Handles concurrent workers | Yes - correctly waits for ALL | **No - only waits for ONE** |

#### A.4.2 `taskqueue_poll_is_busy()`

**FreeBSD Implementation** (sys/kern/subr_taskqueue.c:541-551):

```c
int
taskqueue_poll_is_busy(struct taskqueue *queue, struct task *task)
{
    int retval;

    TQ_LOCK(queue);
    retval = task->ta_pending > 0 || task_get_busy(queue, task) != NULL;
    TQ_UNLOCK(queue);

    return (retval);
}
```

**FreeBSD `task_get_busy()` helper** (lines 126-137):

```c
static struct taskqueue_busy *
task_get_busy(struct taskqueue *queue, struct task *task)
{
    struct taskqueue_busy *tb;

    TQ_ASSERT_LOCKED(queue);
    LIST_FOREACH(tb, &queue->tq_active, tb_link) {
        if (tb->tb_running == task)
            return (tb);
    }
    return (NULL);
}
```

**Current DragonFly Implementation** (sys/kern/subr_taskqueue.c:555-565):

```c
int
taskqueue_poll_is_busy(struct taskqueue *queue, struct task *task)
{
    int busy;

    TQ_LOCK(queue);
    busy = task->ta_pending > 0 || task == queue->tq_running;
    TQ_UNLOCK(queue);

    return (busy);
}
```

**Analysis:**

| Aspect | FreeBSD | DragonFly (Current) |
|--------|---------|---------------------|
| Checks pending | Yes | Yes |
| Checks running | Yes - searches `tq_active` LIST | Yes - compares to single `tq_running` |
| Multi-worker support | Yes - finds task in any worker | **No - only finds if it's THE one running** |

#### A.4.3 `taskqueue_run()` / `taskqueue_run_locked()`

**FreeBSD Implementation** (sys/kern/subr_taskqueue.c:483-525):

```c
static void
taskqueue_run_locked(struct taskqueue *queue)
{
    struct epoch_tracker et;
    struct taskqueue_busy tb;      /* Stack-allocated busy tracker */
    struct task *task;
    bool in_net_epoch;
    int pending;

    KASSERT(queue != NULL, ("tq is NULL"));
    TQ_ASSERT_LOCKED(queue);
    tb.tb_running = NULL;
    LIST_INSERT_HEAD(&queue->tq_active, &tb, tb_link);  /* Register tracker */
    in_net_epoch = false;

    while ((task = STAILQ_FIRST(&queue->tq_queue)) != NULL) {
        STAILQ_REMOVE_HEAD(&queue->tq_queue, ta_link);
        if (queue->tq_hint == task)
            queue->tq_hint = NULL;
        pending = task->ta_pending;
        task->ta_pending = 0;
        tb.tb_running = task;           /* Mark which task is running */
        tb.tb_seq = ++queue->tq_seq;    /* Assign sequence number */
        tb.tb_canceling = false;
        TQ_UNLOCK(queue);

        KASSERT(task->ta_func != NULL, ("task->ta_func is NULL"));
        /* Network epoch handling omitted for brevity */
        task->ta_func(task->ta_context, pending);

        TQ_LOCK(queue);
        wakeup(task);  /* Wake anyone waiting on this specific task */
    }
    /* Network epoch cleanup omitted */
    LIST_REMOVE(&tb, tb_link);  /* Unregister tracker */
}
```

**Current DragonFly Implementation** (sys/kern/subr_taskqueue.c:409-437):

```c
static void
taskqueue_run(struct taskqueue *queue, int lock_held)
{
    struct task *task;
    int pending;

    if (lock_held == 0)
        TQ_LOCK(queue);
    while (STAILQ_FIRST(&queue->tq_queue)) {
        task = STAILQ_FIRST(&queue->tq_queue);
        STAILQ_REMOVE_HEAD(&queue->tq_queue, ta_link);
        pending = task->ta_pending;
        task->ta_pending = 0;
        queue->tq_running = task;      /* Single pointer assignment */

        TQ_UNLOCK(queue);
        task->ta_func(task->ta_context, pending);
        queue->tq_running = NULL;      /* Clear single pointer */
        wakeup(task);
        wakeup(queue);  /* Wake drain waiters */
        TQ_LOCK(queue);
    }
    if (lock_held == 0)
        TQ_UNLOCK(queue);
}
```

**Key Difference:** FreeBSD allocates a `taskqueue_busy` structure **on the stack** of each worker thread and inserts it into `tq_active`. This allows tracking **multiple concurrent running tasks**. DragonFly uses a single `tq_running` pointer, which can only track **one task at a time**.

---

### A.5 Locking Mechanism Comparison

| Aspect | DragonFly | FreeBSD |
|--------|-----------|---------|
| Lock type | Always spinlock (`struct spinlock`) | Configurable: sleep mutex (`MTX_DEF`) or spin mutex (`MTX_SPIN`) |
| Lock init | `spin_init()` | `mtx_init()` |
| Sleep with lock | `ssleep()` | `msleep()` or `msleep_spin()` |
| Lock primitives | `spin_lock()`/`spin_unlock()` | `mtx_lock()`/`mtx_unlock()` |
| Fast (spin) option | Always spin | `taskqueue_create_fast()` for spin |

---

### A.6 Complete Taskqueue Usage Audit in DragonFly

#### A.6.1 All `taskqueue_start_threads()` Callers

| File | Thread Count | Uses `drain_all`? | Uses `poll_is_busy`? |
|------|--------------|-------------------|----------------------|
| `sys/compat/linuxkpi/common/src/linux_work.c:679` | **`cpus` (multi-worker)** | **Yes (via macro)** | **Yes** |
| `sys/compat/linuxkpi/common/src/linux_work.c:793` | 1 | Yes | No |
| `sys/netproto/802_11/wlan/ieee80211.c:351` | 1 | No | No |
| `sys/kern/uipc_usrreq.c:1697` | 1 | No | No |
| `sys/kern/subr_firmware.c:493` | 1 | No | No |
| `sys/net/wg/if_wg.c:2968` | 1 | No | No |
| `sys/dev/netif/iwn/if_iwn.c:735` | 1 | **Commented out for DFly** | No |
| `sys/dev/netif/alc/if_alc.c:1565` | 1 | No | No |
| `sys/dev/netif/bwn/bwn/if_bwn.c:854` | 1 | No | No |
| `sys/dev/netif/ath/ath/if_ath.c:758` | 1 | No | No |
| `sys/dev/netif/iwm/if_iwm.c:6089` | 1 | **Commented out for DFly** | No |
| `sys/dev/netif/oce/oce_if.c:714` | 1 | No | No |
| `sys/dev/raid/mpr/mpr_sas.c:795` | 1 | No | No |
| `sys/dev/raid/mps/mps_sas.c:725` | 1 | No | No |
| `sys/dev/raid/mrsas/mrsas_cam.c:146` | 1 | No | No |
| `sys/dev/virtual/amazon/ena/ena.c:795` | 1 | No | No |
| `sys/dev/virtual/amazon/ena/ena.c:3763` | 1 | No | No |
| `sys/dev/virtual/vmware/vmxnet3/if_vmx.c:997` | **`nthreads` (variable)** | No | No |
| `sys/kern/subr_taskqueue.c:738` (per-CPU) | 1 | No | No |

#### A.6.2 vmxnet3 Thread Count Analysis

```c
/* sys/dev/virtual/vmware/vmxnet3/if_vmx.c:985-997 */
static void
vmxnet3_start_taskqueue(struct vmxnet3_softc *sc)
{
    device_t dev;
    int nthreads, error;

    dev = sc->vmx_dev;

    /*
     * The taskqueue is typically not frequently used, so a dedicated
     * thread for each queue is unnecessary.
     */
    nthreads = MAX(1, sc->vmx_ntxqueues / 2);

    error = taskqueue_start_threads(&sc->vmx_tq, nthreads, PI_NET,
        "%s taskq", device_get_nameunit(dev));
```

**vmxnet3 can create multiple workers** (`MAX(1, ntxqueues/2)`), but it **only uses `taskqueue_drain()` for specific tasks**, not `taskqueue_drain_all()`.

#### A.6.3 All `taskqueue_drain_all()` Callers

| File | Line | Context |
|------|------|---------|
| `sys/compat/linuxkpi/common/src/linux_work.c:807` | IRQ work init | LinuxKPI - **multi-worker queue** |
| `sys/kern/subr_gtaskqueue.c:415` | gtaskqueue drain | Group taskqueue (separate implementation) |
| `sys/kern/subr_gtaskqueue.c:815` | Group drain all | Group taskqueue |
| `sys/kern/subr_taskqueue.c:524` | Implementation | N/A |
| `sys/dev/netif/iwn/if_iwn.c:1466` | Driver cleanup | **Commented out for DragonFly** |
| `sys/dev/netif/iwm/if_iwm.c:6673` | Driver cleanup | **Commented out for DragonFly** |

**iwn and iwm drivers explicitly skip `taskqueue_drain_all()` on DragonFly:**

```c
/* sys/dev/netif/iwn/if_iwn.c:1462-1468 */
#if defined(__DragonFly__)
        /* doesn't exist for DFly, DFly drains tasks on free */
#else
        taskqueue_drain_all(sc->sc_tq);
#endif
        taskqueue_free(sc->sc_tq);
```

#### A.6.4 All `taskqueue_poll_is_busy()` Callers

| File | Line | Function | Context |
|------|------|----------|---------|
| `sys/compat/linuxkpi/common/src/linux_work.c:592` | `linux_flush_work()` | LinuxKPI - **multi-worker** |
| `sys/compat/linuxkpi/common/src/linux_work.c:621` | `linux_flush_delayed_work()` | LinuxKPI - **multi-worker** |
| `sys/compat/linuxkpi/common/src/linux_work.c:657` | `work_busy()` | LinuxKPI - **multi-worker** |
| `sys/kern/subr_taskqueue.c:556` | Implementation | N/A |

---

### A.7 LinuxKPI Workqueue Analysis

#### A.7.1 Workqueue Creation (linux_work.c:670-687)

```c
struct workqueue_struct *
alloc_workqueue(const char *name, unsigned flags, int cpus)
{
    struct workqueue_struct *wq;

    if (cpus == 0)
        cpus = linux_default_wq_cpus;  /* mp_ncpus + 1, minimum 4 */

    wq = kmalloc(sizeof(*wq), M_WAITOK | M_ZERO);
    wq->taskqueue = taskqueue_create(name, M_WAITOK,
        taskqueue_thread_enqueue, &wq->taskqueue);
    atomic_set(&wq->draining, 0);
#ifdef __DragonFly__
    taskqueue_start_threads(&wq->taskqueue, cpus, PWAIT, -1, "%s", name);
#else
    taskqueue_start_threads(&wq->taskqueue, cpus, PWAIT, "%s", name);
#endif
    TAILQ_INIT(&wq->exec_head);
    mtx_init(&wq->exec_mtx, "linux_wq_exec", NULL, MTX_DEF);

    return (wq);
}
```

**Key:** LinuxKPI workqueues create **`cpus` worker threads** (typically `mp_ncpus + 1`).

#### A.7.2 Default Worker Count (linux_work.c:717-727)

```c
static void
linux_work_init(void *arg)
{
    int max_wq_cpus = mp_ncpus + 1;

    /* avoid deadlock when there are too few threads */
    if (max_wq_cpus < 4)
        max_wq_cpus = 4;

    /* set default number of CPUs */
    linux_default_wq_cpus = max_wq_cpus;

    linux_system_short_wq = alloc_workqueue("linuxkpi_short_wq", 0, max_wq_cpus);
    linux_system_long_wq = alloc_workqueue("linuxkpi_long_wq", 0, max_wq_cpus);
    /* ... */
}
```

**Example:** On a 4-core system:
- `linux_system_short_wq` → 5 worker threads
- `linux_system_long_wq` → 5 worker threads

#### A.7.3 flush_workqueue and drain_workqueue Macros (workqueue.h:171-178)

```c
#define flush_workqueue(wq) \
    taskqueue_drain_all((wq)->taskqueue)

#define drain_workqueue(wq) do {        \
    atomic_inc(&(wq)->draining);        \
    taskqueue_drain_all((wq)->taskqueue);   \
    atomic_dec(&(wq)->draining);        \
} while (0)
```

**These macros call `taskqueue_drain_all()` on multi-worker taskqueues.**

---

### A.8 Multi-Worker Problem Illustration

#### A.8.1 Scenario: 5-Worker Taskqueue with Concurrent Tasks

```
Worker Thread 1: Executing task A
Worker Thread 2: Executing task B  
Worker Thread 3: Executing task C
Worker Thread 4: Executing task D
Worker Thread 5: Executing task E  ← This one is tq_running
```

#### A.8.2 DragonFly `taskqueue_poll_is_busy(task A)` - WRONG

```c
busy = task->ta_pending > 0 || task == queue->tq_running;
/* task A != queue->tq_running (which is task E) */
/* Returns: FALSE (WRONG - task A is actually running!) */
```

#### A.8.3 FreeBSD `taskqueue_poll_is_busy(task A)` - CORRECT

```c
retval = task->ta_pending > 0 || task_get_busy(queue, task) != NULL;
/* task_get_busy() searches tq_active list and finds task A */
/* Returns: TRUE (CORRECT) */
```

#### A.8.4 DragonFly `taskqueue_drain_all()` - WRONG

```c
while ((task = STAILQ_FIRST(&queue->tq_queue)) != NULL ||
    queue->tq_running != NULL) {
    /* ... */
}
/* Only waits for queue->tq_running (task E) */
/* Tasks A, B, C, D may still be executing when function returns! */
```

#### A.8.5 FreeBSD `taskqueue_drain_all()` - CORRECT

```c
(void)taskqueue_drain_tq_queue(queue);   /* Wait for pending tasks */
(void)taskqueue_drain_tq_active(queue);  /* Wait for ALL active tasks */
/* Iterates through tq_active list, waits for A, B, C, D, E */
```

---

### A.9 Recent Commit History for DragonFly Taskqueue

```
3f8958c72a Fix taskqueue_drain_all() - use queue wakeup
7466ae8e13 Fix taskqueue_drain_all() - wait on tasks, not queue
7c7fd30e63 Fix race condition in taskqueue_drain_all()
80f0e9f718 Add taskqueue_drain_all() and taskqueue_poll_is_busy() implementations
```

**These commits attempted iterative fixes but did not address the fundamental architectural issue: DragonFly lacks multi-task tracking.**

#### A.9.1 Commit 80f0e9f718 (Original Addition)

Added `taskqueue_drain_all()` and `taskqueue_poll_is_busy()` to DragonFly kernel. Removed stubs from `dragonfly_compat.h`.

#### A.9.2 Commits 7c7fd30e63, 7466ae8e13, 3f8958c72a (Fix Attempts)

Multiple attempts to fix wakeup/sleep race conditions in `taskqueue_drain_all()`. **None addressed the multi-worker tracking issue.**

**Recommendation:** Revert to commit 80f0e9f718 or earlier before implementing proper fix.

---

### A.10 Conclusions

#### A.10.1 Native DragonFly Usage

| Finding | Details |
|---------|---------|
| All native drivers use single-worker | Every `taskqueue_start_threads()` call uses count=1 (except vmxnet3 variable) |
| vmxnet3 multi-worker safe | Uses only `taskqueue_drain()` for specific tasks, not `drain_all` |
| iwn/iwm skip drain_all | Explicitly commented out for DragonFly |
| Current tq_running sufficient | For all native code |

#### A.10.2 LinuxKPI Usage

| Finding | Details |
|---------|---------|
| Creates multi-worker queues | `cpus = mp_ncpus + 1` (minimum 4) |
| Uses drain_all | Via `flush_workqueue()` and `drain_workqueue()` macros |
| Uses poll_is_busy | Via `flush_work()`, `flush_delayed_work()`, `work_busy()` |
| Current implementation broken | Will cause races, use-after-free |

#### A.10.3 Required for LinuxKPI/DRM

To properly support LinuxKPI workqueues with multi-worker taskqueues:

1. **Add `struct taskqueue_busy`** - Per-worker task tracking structure
2. **Add `tq_active` list** - Track all currently executing tasks
3. **Add `tq_seq`** - Sequence number for drain ordering
4. **Implement `task_get_busy()`** - Search active list for task
5. **Modify `taskqueue_run()`** - Use stack-allocated `taskqueue_busy`
6. **Implement `taskqueue_drain_tq_queue()`** - Barrier task pattern
7. **Implement `taskqueue_drain_tq_active()`** - Sequence-based wait

#### A.10.4 Alternative Approaches

| Approach | Pros | Cons |
|----------|------|------|
| Full FreeBSD port | Complete compatibility | More invasive changes |
| Minimal tq_active tracking | Fixes multi-worker issue | Missing barrier optimization |
| Force single-worker for LinuxKPI | No kernel changes | **Major GPU performance penalty** |
| Keep current (broken) | No work | **LinuxKPI will crash/corrupt** |

---

### A.11 Source File References

#### A.11.1 DragonFly Files

- `sys/kern/subr_taskqueue.c` - Main taskqueue implementation (744 lines)
- `sys/sys/taskqueue.h` - Public header (179 lines)
- `sys/kern/subr_gtaskqueue.c` - Group taskqueue (separate implementation)
- `sys/compat/linuxkpi/common/src/linux_work.c` - LinuxKPI workqueue wrapper
- `sys/compat/linuxkpi/common/include/linux/workqueue.h` - Workqueue macros

#### A.11.2 FreeBSD Files (~/s/freebsd)

- `sys/kern/subr_taskqueue.c` - Main taskqueue implementation
- `sys/sys/taskqueue.h` - Public header
- `sys/sys/_task.h` - Task structure definition

---

### A.12 Raw Data: All taskqueue_start_threads Calls

```
sys/compat/linuxkpi/common/src/linux_work.c:679:	taskqueue_start_threads(&wq->taskqueue, cpus, PWAIT, -1, "%s", name);
sys/compat/linuxkpi/common/src/linux_work.c:681:	taskqueue_start_threads(&wq->taskqueue, cpus, PWAIT, "%s", name);
sys/compat/linuxkpi/common/src/linux_work.c:793:	taskqueue_start_threads(&linux_irq_work_tq, 1, PWAIT, -1,
sys/compat/linuxkpi/common/src/linux_work.c:796:	taskqueue_start_threads(&linux_irq_work_tq, 1, PWAIT,
sys/netproto/802_11/wlan/ieee80211.c:351:	taskqueue_start_threads(&ic->ic_tq, 1, TDPRI_KERN_DAEMON, -1,
sys/netproto/802_11/wlan/ieee80211.c:354:	taskqueue_start_threads(&ic->ic_tq, 1, PI_NET, "%s net80211 taskq",
sys/kern/uipc_usrreq.c:1697:	taskqueue_start_threads(&unp_taskqueue, 1, TDPRI_KERN_DAEMON,
sys/kern/subr_firmware.c:493:		(void) taskqueue_start_threads(&firmware_tq, 1, TDPRI_KERN_DAEMON,
sys/net/wg/if_wg.c:2968:		taskqueue_start_threads(&wg_taskqueues[i], 1,
sys/dev/netif/iwn/if_iwn.c:735:	error = taskqueue_start_threads(&sc->sc_tq, 1, TDPRI_KERN_DAEMON,
sys/dev/netif/iwn/if_iwn.c:738:	error = taskqueue_start_threads(&sc->sc_tq, 1, 0, "iwn_taskq");
sys/dev/netif/alc/if_alc.c:1565:	taskqueue_start_threads(&sc->alc_tq, 1, TDPRI_KERN_DAEMON, -1, "%s taskq",
sys/dev/netif/bwn/bwn/if_bwn.c:854:	taskqueue_start_threads(&sc->sc_tq, 1, TDPRI_KERN_DAEMON,
sys/dev/netif/bwn/bwn/if_bwn.c:859:	taskqueue_start_threads(&sc->sc_tq, 1, PI_NET,
sys/dev/netif/ath/ath/if_ath.c:758:	taskqueue_start_threads(&sc->sc_tq, 1, TDPRI_KERN_DAEMON, -1,
sys/dev/netif/ath/ath/if_ath.c:763:	taskqueue_start_threads(&sc->sc_tq, 1, PI_NET, "%s taskq",
sys/dev/netif/iwm/if_iwm.c:6089:	error = taskqueue_start_threads(&sc->sc_tq, 1, TDPRI_KERN_DAEMON, -1, "iwm_taskq");
sys/dev/netif/oce/oce_if.c:714:	taskqueue_start_threads(&ii->tq, 1, TDPRI_KERN_DAEMON, -1, "%s taskq",
sys/dev/raid/mpr/mpr_sas.c:795:	taskqueue_start_threads(&sassc->ev_tq, 1, TDPRI_KERN_DAEMON,
sys/dev/raid/mps/mps_sas.c:725:	taskqueue_start_threads(&sassc->ev_tq, 1, TDPRI_KERN_DAEMON,
sys/dev/raid/mrsas/mrsas_cam.c:146:    taskqueue_start_threads(&sc->ev_tq, 1, TDPRI_KERN_DAEMON,
sys/dev/virtual/amazon/ena/ena.c:795:	taskqueue_start_threads_cpuset(&rx_ring->cmpl_tq, 1, PI_NET, &cpu_mask,
sys/dev/virtual/amazon/ena/ena.c:799:	taskqueue_start_threads(&rx_ring->cmpl_tq, 1, TDPRI_KERN_DAEMON, -1,
sys/dev/virtual/amazon/ena/ena.c:3763:	taskqueue_start_threads(&adapter->reset_tq, 1, TDPRI_KERN_DAEMON, -1,
sys/dev/virtual/vmware/vmxnet3/if_vmx.c:997:	error = taskqueue_start_threads(&sc->vmx_tq, nthreads, PI_NET,
sys/kern/subr_taskqueue.c:738:		taskqueue_start_threads(&taskqueue_thread[cpu], 1,
```

---

### A.13 Raw Data: All drain_all and poll_is_busy Calls

```
sys/compat/linuxkpi/common/src/linux_work.c:592:		retval = taskqueue_poll_is_busy(tq, &work->work_task);
sys/compat/linuxkpi/common/src/linux_work.c:621:		retval = taskqueue_poll_is_busy(tq, &dwork->work.work_task);
sys/compat/linuxkpi/common/src/linux_work.c:657:		return (taskqueue_poll_is_busy(tq, &work->work_task));
sys/compat/linuxkpi/common/src/linux_work.c:807:	taskqueue_drain_all(linux_irq_work_tq);
sys/kern/subr_gtaskqueue.c:415:gtaskqueue_drain_all(struct gtaskqueue *queue)
sys/kern/subr_gtaskqueue.c:815:		gtaskqueue_drain_all(q);
sys/kern/subr_taskqueue.c:524:taskqueue_drain_all(struct taskqueue *queue)
sys/kern/subr_taskqueue.c:556:taskqueue_poll_is_busy(struct taskqueue *queue, struct task *task)
sys/dev/netif/iwn/if_iwn.c:1466:		taskqueue_drain_all(sc->sc_tq);
sys/dev/netif/iwm/if_iwm.c:6673:		taskqueue_drain_all(sc->sc_tq);
```

**Note:** iwn and iwm calls are inside `#if !defined(__DragonFly__)` blocks.
