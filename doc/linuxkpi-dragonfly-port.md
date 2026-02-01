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

**Goal:** Implement proper `taskqueue_drain_all()` and `taskqueue_poll_is_busy()` for DragonFly.

**Approach:**
1. **Study FreeBSD implementation** in `/sys/kern/subr_taskqueue.c`
2. **Implement in DragonFly kernel** (`sys/kern/subr_taskqueue.c`):
   - Add `taskqueue_drain_all()` that drains both queue and active tasks
   - Add `taskqueue_poll_is_busy()` that checks pending and active status
3. **Remove stubs** from `dragonfly_compat.h`
4. **Test** with workqueue test suite (should pass with 100+ items)

**Key Implementation Details from FreeBSD:**
- `taskqueue_drain_tq_queue()`: Drains tasks from pending queue
- `taskqueue_drain_tq_active()`: Waits for and drains currently executing tasks
- `task_get_busy()`: Checks if task is currently executing on a thread

**Files to Modify:**
- `sys/kern/subr_taskqueue.c` - Add real implementations
- `sys/sys/taskqueue.h` - Add function declarations
- `sys/compat/linuxkpi/common/include/linux/dragonfly_compat.h` - Remove stubs

**Success Criteria:**
- Workqueue tests pass with 100+ concurrent work items
- `flush_workqueue()` and `drain_workqueue()` properly wait for completion
- No race conditions in work re-queuing scenarios

#### Phase 3B: Fix Device Operations (DRM Critical)

**Goal:** Implement proper `linuxdev_ops` for DRM device access.

**Approach:**
1. **Study FreeBSD's linux_compat.c** device operations implementation
2. **Implement DragonFly equivalents:**
   - `linux_dev_open()` - Map to DragonFly device open
   - `linux_dev_close()` - Map to DragonFly device close
   - `linux_dev_ioctl()` - Map to DragonFly ioctl with Linux compatibility
   - `linux_dev_mmap()` - Map to DragonFly mmap
   - `linux_dev_read/write()` - If needed by DRM
3. **Map Linux device model** to DragonFly's device structure

**Files to Modify:**
- `sys/compat/linuxkpi/common/src/linux_compat.c` - Implement device ops

**Success Criteria:**
- DRM device nodes can be opened/closed
- DRM ioctls work correctly
- GPU device can be accessed by userspace

#### Phase 3C: Fix VM and Memory Stubs (HIGH PRIORITY)

**Goal:** Implement `vm_radix_*`, `vm_reserv_*`, and `sf_buf_kva()` properly.

**Approach:**
1. **For vm_radix:** Study FreeBSD's VM radix tree implementation; either:
   - Port FreeBSD's implementation to DragonFly, OR
   - Use DragonFly's existing radix tree (if available) with LinuxKPI wrapper
2. **For vm_reserv:** Study FreeBSD's reservation system; implement DragonFly version
3. **For sf_buf_kva:** Fix to return proper kernel virtual address

**Alternative for vm_radix/vm_reserv:**
If DragonFly doesn't have equivalent functionality, DRM drivers might work with fallback implementations that use basic malloc/free with tracking.

**Files to Modify:**
- `sys/compat/linuxkpi/common/include/vm/vm_radix.h`
- `sys/compat/linuxkpi/common/include/vm/vm_reserv.h`
- `sys/compat/linuxkpi/common/include/sys/sf_buf.h`

#### Phase 3D: Fix Remaining HIGH Priority Stubs

**Goal:** Fix UMA, per-CPU, VGA arbitration, and stack tracing stubs.

**Priority Order:**
1. **sf_buf_kva()** - Critical for DMA (do with Phase 3C)
2. **uma_zcreate/alloc/free** - Performance optimization (can use malloc fallback for now)
3. **pcpu_alloc** - Performance optimization (can use regular allocation for now)
4. **vga_get/put** - Only needed for multi-GPU systems
5. **stack_save** - Debug feature only

**Approach:** Implement following FreeBSD patterns where applicable, or document that fallback behavior is acceptable for GPU/DRM use case.

### Testing and Validation

#### Phase 3A Testing (Taskqueue)
```bash
# Build kernel with new taskqueue implementations
cd /usr/src
make -j$(sysctl -n hw.ncpu) buildkernel KERNCONF=X86_64_GENERIC

# Run workqueue tests
cd /usr/src/test/testcases
dfregress linuxkpi/workqueue/workqueue.run

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
- ⏳ Critical stub analysis completed; implementation pending
- ⏳ Taskqueue drain implementation (Phase 3A)

### Blocked/Pending
- ⏸ Device operations implementation (Phase 3B) - depends on Phase 3A for testing
- ⏸ VM radix/reserv implementation (Phase 3C) - investigate if DRM actually needs these
- ⏸ DRM-KMOD build attempt - blocked until Phase 3A complete

### Next Steps
1. **Implement `taskqueue_drain_all()`** following FreeBSD's approach
2. **Implement `taskqueue_poll_is_busy()`** following FreeBSD's approach
3. **Remove stubs** from `dragonfly_compat.h`
4. **Test** workqueue tests with high load (100+ items)
5. **Validate** with drm-kmod build once taskqueue works
