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

## Test Objectives

1. **Determine if GGTT is the corruption source**
   - If crash still occurs with GGTT disabled → corruption happens earlier (step 7 or before)
   - If crash stops → GGTT code is the culprit

2. **Determine if GPU reset is the corruption source**
   - sanitize_gpu() disabled → no GPU reset via GEN6_GDRST
   - If crash stops → GPU reset was triggering DMA corruption

3. **Verify VT-d detection**
   - Debug output will show:
     - `VT-d active` - should be 1 (true) if VM detection works
     - `Run as guest` - should be 1 (true) in VM
     - `device_iommu_mapped` - will be 0 (always false on DragonFly)

## Expected Outcomes

### Scenario A: Crash Still Occurs
If kernel crashes even with GGTT and GPU reset disabled:
- **Conclusion:** Corruption happens in step 7 or earlier
- **Next step:** Investigate `intel_uncore_init_mmio` and forcewake initialization
- **Suspect:** MMIO register writes going to wrong addresses

### Scenario B: Crash Stops
If driver loads successfully:
- **Conclusion:** Either GGTT code or GPU reset was causing corruption
- **Next step:** Re-enable components one by one to isolate which one

### Scenario C: Debug Output Shows Issues
If debug output shows:
- `VT-d active=0` and `Run as guest=0` in a VM
  - **Issue:** Hypervisor detection not working
  - **Impact:** Driver doesn't know it's in a VM
  
- `device_iommu_mapped=0` (always)
  - **Issue:** Known limitation
  - **Impact:** Missing IOMMU workarounds

## Success Criteria

1. **Short-term:** Driver loads without crashing (even if display doesn't work)
2. **Medium-term:** Identify exact function causing corruption
3. **Long-term:** Determine if issue is VM-specific or driver bug

## Critical Questions to Answer

1. Does disabling ALL GGTT functions prevent the crash?
2. Does disabling GPU reset prevent the crash?
3. Are PCI BARs correctly configured in the VM?
4. Is VT-d/IOMMU properly configured for passthrough?
5. Is this reproducible on real hardware (not VM)?

## Next Steps Based on Results

### If Test 1 Succeeds (No Crash)
- Re-enable GPU reset → test
- If crash returns → GPU reset is culprit
- If no crash → re-enable GGTT functions one by one

### If Test 1 Fails (Still Crashes)
- Add debug output to step 7 functions
- Check forcewake register addresses
- Verify BAR mapping in VM
- Consider real hardware test

### If VT-d Detection Fails
- Implement proper VT-d detection in DragonFly LinuxKPI
- Check if hypervisor detection works
- May need VM-specific workarounds

## Files Modified

**drm-kmod repo:**
- `drivers/gpu/drm/i915/i915_driver.c`
  - Disabled sanitize_gpu() GPU reset
  - Disabled all GGTT operations in i915_driver_hw_probe()
  - Added VT-d/guest detection debug output

- `drivers/gpu/drm/i915/gt/intel_ggtt.c`
  - Disabled i915_ggtt_probe_hw()
  - Disabled i915_ggtt_init_hw()

## Notes

- All original code preserved with `#if 0` blocks for easy restoration
- Changes are for debugging only - not a permanent fix
- Need to test on VM with PCI passthrough
- May need to test on real hardware to determine if VM-specific
