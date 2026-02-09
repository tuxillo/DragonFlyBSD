# i915 Driver Memory Corruption Investigation Report

**Date:** 2026-02-09  
**Issue:** Critical memory corruption causing kernel crashes during i915 driver probe in DragonFly BSD VM passthrough environment  
**Hardware:** Intel Alder Lake-N (UHD Graphics) - PCI passthrough to QEMU/KVM VM  

## Executive Summary

The DragonFly BSD i915 driver experiences random memory corruption during probe, manifesting as:
- Kernel crashes at random addresses with garbage instructions (e.g., `adcb %dh,-0x7d8e(%rdi)`)
- Stack traces showing NULL function pointers
- DDB debugger crashing after driver load
- Even panic messages becoming garbled

The corruption appears to happen during or immediately after step 7 (`i915_driver_mmio_probe`), with symptoms manifesting at step 8 (`i915_driver_hw_probe`).

## Key Findings

### 1. Critical Bug in LinuxKPI: VT-d Detection Always Fails

**Location:** `dragonfly/sys/compat/linuxkpi/common/include/linux/device.h:608`

```c
static inline bool
device_iommu_mapped(struct device *dev __unused)
{
	return (false);  // ALWAYS returns false!
}
```

**Impact:** The i915 driver cannot detect VT-d/IOMMU status on DragonFly BSD. This causes:
- Missing VT-d workarounds (e.g., BXT GGTT workaround)
- Driver assumes no IOMMU protection when IOMMU may be active
- Potential DMA synchronization issues

The detection falls back to `i915_run_as_guest()` which uses x86 hypervisor detection:

```c
static inline bool i915_run_as_guest(void)
{
#if IS_ENABLED(CONFIG_X86)
	return !hypervisor_is_type(X86_HYPER_NATIVE);
#else
	return false;
#endif
}
```

### 2. GPU Reset in sanitize_gpu() - HIGH RISK

**Location:** `drm-kmod/drivers/gpu/drm/i915/i915_driver.c:194-203`

```c
static void sanitize_gpu(struct drm_i915_private *i915)
{
	if (!INTEL_INFO(i915)->gpu_reset_clobbers_display) {
		struct intel_gt *gt;
		unsigned int i;

		for_each_gt(gt, i915, i)
			__intel_gt_reset(gt, ALL_ENGINES);  // Triggers GEN6_GDRST
	}
}
```

**Called from:** Step 7 (i915_driver_mmio_probe:346)

**Risk:** GPU reset via `GEN6_GDRST` (0x941C) occurs:
- AFTER MMIO is initialized
- BEFORE DMA masks are set (happens in step 8)
- If GPU was in bad state from VM passthrough, reset could trigger DMA operations
- DMA to uninitialized addresses = memory corruption

### 3. Forcewake Domain Initialization

**Location:** `drm-kmod/drivers/gpu/drm/i915/intel_uncore.c:2555-2619`

Gen12 (Alder Lake) initializes multiple forcewake domains:
- `FORCEWAKE_RENDER` (0x2000-0x26ff, 0x2800-0x2aff)
- `FORCEWAKE_GT` (0x2700-0x27ff, 0x4000-0x51ff)
- Multiple `FORCEWAKE_MEDIA_VDBOX*` domains

Critical registers written:
- `FORCEWAKE_GT_GEN9` (0xA188)
- `FORCEWAKE_ACK_GT_GEN9` (0x130044)

**Risk:** If BAR mapping is wrong or VT-d/IOMMU not properly isolating GPU:
- Forcewake writes could go to system memory instead of GPU MMIO
- GPU powers up incorrectly and DMAs to random addresses

### 4. MMIO Function Pointer Setup

**Location:** `drm-kmod/drivers/gpu/drm/i915/intel_uncore.c:2588-2591`

```c
} else if (GRAPHICS_VER(i915) >= 12) {
	ASSIGN_FW_DOMAINS_TABLE(uncore, __gen12_fw_ranges);
	ASSIGN_SHADOW_TABLE(uncore, gen12_shadowed_regs);
	ASSIGN_WRITE_MMIO_VFUNCS(uncore, fwtable);
```

All subsequent MMIO writes use these function pointers. If corrupted, writes go to random addresses.

### 5. Current Disabled Workarounds

Based on previous debugging, these have been disabled:
- `io_mapping_init_wc` - Maps 256MB GMADR aperture (known corruption source)
- `intel_gmbus_setup` - I2C/GMBus initialization (crashes)
- `intel_crtc_init` - Display pipe initialization (crashes)
- MSI setup - Disabled
- DRAM detection and BW init - Disabled

### 6. DMA Mask Setup Timing

**Location:** `drm-kmod/drivers/gpu/drm/i915/i915_driver.c:376-418`

```c
ret = dma_set_mask(i915->drm.dev, DMA_BIT_MASK(mask_size));
ret = dma_set_coherent_mask(i915->drm.dev, DMA_BIT_MASK(mask_size));
```

For Alder Lake N: DMA mask size is 39 bits.

**Critical:** DMA masks are set in step 8, but GPU reset happens in step 7. If GPU initiates DMA during/after reset but before masks are set, it could write to any physical address.

## Suspected Root Causes (Ordered by Likelihood)

### 1. VM PCI Passthrough DMA Issues (Most Likely)
- QEMU/Proxmox not properly isolating PCI BARs
- DMA writes from GPU landing in wrong physical memory
- IOMMU/VT-d translation issues between guest and host
- **Evidence:** Corruption is random, inconsistent between runs, severe enough to corrupt kernel data structures

### 2. GPU Reset Triggering Unauthorized DMA
- `sanitize_gpu()` reset occurs before DMA masks are configured
- GPU may DMA to random physical addresses during/after reset
- **Evidence:** Corruption manifests after step 7 (where reset happens)

### 3. MMIO Mapping Corruption
- Forcewake register writes going to wrong addresses
- Function pointer corruption in uncore vfuncs
- **Evidence:** Stack traces show NULL function pointers

### 4. DragonFly LinuxKPI Compatibility Issues
- `device_iommu_mapped()` always returns false
- Missing VT-d workarounds applied
- **Evidence:** Driver cannot detect VT-d status

## Test 1: Isolate Corruption Source

**Changes Made:**

### 1. Disabled GGTT Functions
**File:** `drivers/gpu/drm/i915/gt/intel_ggtt.c`

```c
int i915_ggtt_probe_hw(struct drm_i915_private *i915)
{
	pr_info("i915_ggtt_probe_hw: TEST 1 - SKIPPED, returning 0\n");
	return 0;
	// Original code preserved with #if 0
}

int i915_ggtt_init_hw(struct drm_i915_private *i915)
{
	pr_info("i915_ggtt_init_hw: TEST 1 - SKIPPED, returning 0\n");
	return 0;
	// Original code preserved with #if 0
}
```

### 2. Disabled All GGTT Operations in Step 8
**File:** `drivers/gpu/drm/i915/i915_driver.c:485-540`

Wrapped all GGTT calls in `#if 0`:
- `i915_ggtt_probe_hw`
- `i915_ggtt_init_hw`
- `intel_gt_tiles_init`
- `intel_memory_regions_hw_probe`
- `i915_ggtt_enable_hw`

### 3. Disabled GPU Reset
**File:** `drivers/gpu/drm/i915/i915_driver.c:194-203`

```c
static void sanitize_gpu(struct drm_i915_private *i915)
{
	pr_info("sanitize_gpu: SKIPPING GPU reset for VM passthrough test\n");
#if 0
	// Original GPU reset code disabled
#endif
}
```

### 4. Added Critical Debug Output
**File:** `drivers/gpu/drm/i915/i915_driver.c:812-818`

```c
/* CRITICAL DEBUG: Check VT-d and guest detection */
pr_info("i915_driver_probe: VT-d active=%d, Run as guest=%d\n",
	i915_vtd_active(i915), i915_run_as_guest());
pr_info("i915_driver_probe: device_iommu_mapped=%d\n",
	device_iommu_mapped(i915->drm.dev));
```

## Test Results

### FreeBSD Test - SUCCESS!
**Date:** 2026-02-10
**Result:** FreeBSD 14.x with identical VM passthrough setup loads i915 driver successfully

**FreeBSD dmesg output:**
```
drmn1: <drmn> on vgapci1
vgapci1: child drmn1 requested pci_enable_io
vgapci1: ROM mapping failed
drmn1: [drm] Failed to find VBIOS tables (VBT)
drmn1: successfully loaded firmware image 'i915/adlp_dmc.bin'
drmn1: [drm] Finished loading DMC firmware i915/adlp_dmc.bin (v2.20)
lkpi_iic0-8: <LinuxKPI I2C> on drmn1
drmn1: [drm] Cannot find any crtc or sizes
[drm] Initialized i915 1.6.0 20201103 for drmn1 on minor 0
```

**Key observations:**
- No memory corruption or panic
- DMC firmware loaded successfully
- 9 I2C/GMBus buses initialized
- Driver initialized (display non-functional due to VM, but no crash)

**Conclusion:** This is NOT a fundamental BSD + VM passthrough issue. FreeBSD works with the same hardware and VM configuration.

### DragonFly vs FreeBSD Comparison

**VM Configuration (identical):**
- Proxmox/QEMU with Intel Alder Lake-N UHD Graphics passthrough
- BAR 0: Memory at 0xfb000000, size 16MB
- BAR 2: Prefetchable Memory at 0xe0000000, size 256MB
- No IOMMU enabled in guest (neither OS shows IOMMU/VT-d messages)

**PCI BAR Information:**
```
vgapci0@pci0:1:0:0:
    bar   [10] = type Memory, range 64, base 0xfb000000, size 16777216, enabled
    bar   [18] = type Prefetchable Memory, range 64, base 0xe0000000, size 268435456, enabled
    bar   [20] = type I/O Port, range 32, base 0x5000, size 64, enabled
```

**Critical Difference:**
FreeBSD's LinuxKPI and base system handle something correctly that DragonFly doesn't. The issue is DragonFly-specific.

## Root Cause Analysis

### Eliminated Suspects:
1. ❌ **VM PCI Passthrough hardware issue** - FreeBSD works with identical setup
2. ❌ **IOMMU/VT-d requirement** - Neither OS has IOMMU enabled in guest
3. ❌ **Fundamental BSD incompatibility** - FreeBSD proves it works

### Remaining Suspects:
1. **DragonFly LinuxKPI differences** - PCI handling, memory mapping, or initialization
2. **DragonFly base system differences** - PCI bus, BAR mapping, or interrupt handling
3. **Timing/race conditions** - DragonFly may initialize components in different order

## Critical Code Differences Found

### PCI Express Definitions
DragonFly has extensive compatibility mappings in `linux/pci.h` for PCI Express register names:
```c
#ifdef __DragonFly__
/* PCIEM_FLAGS_* -> Use DragonFly's naming */
#ifndef PCIEM_FLAGS_VERSION
#define PCIEM_FLAGS_VERSION	PCIEM_CAP_VER_MASK
#endif
/* ... many more mappings ... */
#endif
```

FreeBSD doesn't need these mappings, suggesting DragonFly's PCI layer uses different naming conventions.

### i915 Driver Initialization
Both use the same drm-kmod repo (DragonFly forked from FreeBSD's), but:
- **FreeBSD**: Loads successfully through full probe sequence
- **DragonFly**: Crashes at Step 9 (intel_display_driver_probe_noirq) or earlier

## Current Debug State

### Fixes Applied:
1. ✅ **DRM core initialization** - Added `__DragonFly__` to module_init_order
2. ✅ **GPU reset disabled** - sanitize_gpu() early return prevents DMA corruption
3. ✅ **Step-by-step debug** - Added pr_info at steps 6-16

### Current Crash Location:
Step 9 (`intel_display_driver_probe_noirq`) crashes immediately after step 8 completes:
```
i915: Step 8 - i915_driver_hw_probe done
i915: Step 9 - intel_display_driver_probe_noirq starting
Fatal trap 1: privileged instruction fault while in kernel mode
```

## Next Investigation Steps

### 1. Compare DragonFly vs FreeBSD LinuxKPI
**Files to compare:**
- `sys/compat/linuxkpi/common/include/linux/pci.h`
- `sys/compat/linuxkpi/common/src/linux_pci.c`
- `sys/compat/linuxkpi/common/include/linux/device.h`
- `sys/compat/linuxkpi/common/src/linux_device.c`

**Focus on:**
- PCI resource handling
- BAR mapping functions  
- DMA mask setting
- Memory mapping (ioremap)

### 2. Check DragonFly PCI Bus Implementation
**Files to examine:**
- `sys/bus/pci/pci.c`
- `sys/bus/pci/pcivar.h`
- `sys/x86/pci/pci_early_quirks.c`

**Look for:**
- Differences in BAR allocation
- PCI Express register access
- Resource mapping differences

### 3. Verify MMIO Mapping
The crash happens after step 7 (MMIO probe) but during step 8/9. This suggests:
- MMIO registers mapped incorrectly
- Offsets calculated wrong
- BAR addresses translated incorrectly

**Specific checks:**
- `intel_uncore_setup_mmio()` - maps 2MB at BAR0 base
- `ggtt_probe_common()` - maps GTT page tables at BAR0 + 8MB
- Verify `pci_resource_start()` returns correct address (0xfb000000)

## Files Modified

**drm-kmod repo (6.6-lts-dfly branch):**
- `drivers/gpu/drm/drm_drv.c` - Added `__DragonFly__` to module_init_order
- `drivers/gpu/drm/i915/i915_driver.c` - Disabled GPU reset, added step debug
- `drivers/gpu/drm/i915/gt/intel_ggtt.c` - Disabled GGTT functions (reverted)

**dragonfly repo (port-linuxkpi branch):**
- `doc/i915-startup-protection-fault.md` - This document

## Key Insight

Since FreeBSD works with identical hardware and VM configuration, the issue is in DragonFly's:
1. LinuxKPI compatibility layer
2. PCI bus implementation
3. Memory mapping functions
4. Or timing/initialization order

The corruption is NOT caused by:
- VM passthrough being fundamentally broken
- Missing IOMMU (FreeBSD also doesn't have it)
- Hardware incompatibility

**Action Required:** Systematic comparison of DragonFly vs FreeBSD implementations to find the divergence point.

## References

- FreeBSD drm-kmod: https://github.com/freebsd/drm-kmod
- DragonFly LinuxKPI: `sys/compat/linuxkpi/`
- FreeBSD LinuxKPI: `sys/compat/linuxkpi/common/`
